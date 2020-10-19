/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "chre/platform/host_link.h"

#include "chre/core/event_loop_manager.h"
#include "chre/core/host_comms_manager.h"
#include "chre/core/settings.h"
#include "chre/platform/aoc/system_time.h"
#include "chre/platform/shared/host_protocol_chre.h"
#include "chre/platform/shared/nanoapp_load_manager.h"
#include "chre/util/fixed_size_blocking_queue.h"
#include "chre/util/flatbuffers/helpers.h"
#include "chre_api/chre/version.h"

namespace chre {

Nanoseconds HostLinkBase::mLastTimeSyncRequestNanos(0);

namespace {

//! Used to pass the client ID through the user data pointer in deferCallback
union HostClientIdCallbackData {
  uint16_t hostClientId;
  void *ptr;
};
static_assert(sizeof(uint16_t) <= sizeof(void *),
              "Pointer must at least fit a u16 for passing the host client ID");

struct LoadNanoappCallbackData {
  uint64_t appId;
  uint32_t transactionId;
  uint16_t hostClientId;
  UniquePtr<Nanoapp> nanoapp;
  uint32_t fragmentId;
};

struct UnloadNanoappCallbackData {
  uint64_t appId;
  uint32_t transactionId;
  uint16_t hostClientId;
  bool allowSystemNanoappUnload;
};

inline HostCommsManager &getHostCommsManager() {
  return EventLoopManagerSingleton::get()->getHostCommsManager();
}

void setTimeSyncRequestTimer(Nanoseconds delay) {
  static TimerHandle sHandle;
  static bool sHandleInitialized;

  if (sHandleInitialized) {
    EventLoopManagerSingleton::get()->cancelDelayedCallback(sHandle);
  }

  auto callback = [](uint16_t /* eventType */, void * /* data */) {
    HostLinkBase::sendTimeSyncRequest();
  };
  sHandle = EventLoopManagerSingleton::get()->setDelayedCallback(
      SystemCallbackType::TimerSyncRequest, nullptr /* data */, callback,
      delay);
  sHandleInitialized = true;
}

void sendDebugDumpData(uint16_t hostClientId, const char *debugStr,
                       size_t debugStrSize) {
  constexpr size_t kFixedSizePortion = 52;
  ChreFlatBufferBuilder builder(kFixedSizePortion + debugStrSize);
  HostProtocolChre::encodeDebugDumpData(builder, hostClientId, debugStr,
                                        debugStrSize);

  getHostCommsManager().send(builder.GetBufferPointer(), builder.GetSize());
}

void sendDebugDumpResponse(uint16_t hostClientId, bool success,
                           uint32_t dataCount) {
  constexpr size_t kFixedSizePortion = 52;
  ChreFlatBufferBuilder builder(kFixedSizePortion);
  HostProtocolChre::encodeDebugDumpResponse(builder, hostClientId, success,
                                            dataCount);
  getHostCommsManager().send(builder.GetBufferPointer(), builder.GetSize());
}

void constructNanoappListCallback(uint16_t /*eventType*/, void *deferCbData) {
  HostClientIdCallbackData clientIdCbData;
  clientIdCbData.ptr = deferCbData;

  struct NanoappListData {
    ChreFlatBufferBuilder *builder;
    DynamicVector<NanoappListEntryOffset> nanoappEntries;
  };
  NanoappListData cbData;

  EventLoop &eventLoop = EventLoopManagerSingleton::get()->getEventLoop();
  size_t expectedNanoappCount = eventLoop.getNanoappCount();
  if (!cbData.nanoappEntries.reserve(expectedNanoappCount)) {
    LOG_OOM();
  } else {
    constexpr size_t kFixedOverhead = 48;
    constexpr size_t kPerNanoappSize = 32;
    size_t initialBufferSize =
        (kFixedOverhead + expectedNanoappCount * kPerNanoappSize);

    ChreFlatBufferBuilder builder(initialBufferSize);
    cbData.builder = &builder;

    auto nanoappAdderCallback = [](const Nanoapp *nanoapp, void *data) {
      auto *listData = static_cast<NanoappListData *>(data);
      HostProtocolChre::addNanoappListEntry(
          *(listData->builder), listData->nanoappEntries, nanoapp->getAppId(),
          nanoapp->getAppVersion(), true /*enabled*/,
          nanoapp->isSystemNanoapp());
    };
    eventLoop.forEachNanoapp(nanoappAdderCallback, &cbData);
    HostProtocolChre::finishNanoappListResponse(builder, cbData.nanoappEntries,
                                                clientIdCbData.hostClientId);
    getHostCommsManager().send(builder.GetBufferPointer(), builder.GetSize());
  }
}

void sendFragmentResponse(uint16_t hostClientId, uint32_t transactionId,
                          uint32_t fragmentId, bool success) {
  constexpr size_t kInitialBufferSize = 52;
  ChreFlatBufferBuilder builder(kInitialBufferSize);
  HostProtocolChre::encodeLoadNanoappResponse(
      builder, hostClientId, transactionId, success, fragmentId);

  if (!getHostCommsManager().send(builder.GetBufferPointer(),
                                  builder.GetSize())) {
    LOGE(
        "Failed to send fragment response for HostClientID: %x FragmentID: %x"
        "transactionID: 0x%x",
        hostClientId, fragmentId, transactionId);
  }
}

void finishLoadingNanoappCallback(uint16_t /*eventType*/, void *data) {
  UniquePtr<LoadNanoappCallbackData> cbData(
      static_cast<LoadNanoappCallbackData *>(data));
  constexpr size_t kInitialBufferSize = 48;
  ChreFlatBufferBuilder builder(kInitialBufferSize);

  CHRE_ASSERT(cbData != nullptr);
  LOGD("Finished loading nanoapp cb on fragment ID: %u", cbData->fragmentId);

  EventLoop &eventLoop = EventLoopManagerSingleton::get()->getEventLoop();
  bool success =
      cbData->nanoapp->isLoaded() && eventLoop.startNanoapp(cbData->nanoapp);

  sendFragmentResponse(cbData->hostClientId, cbData->transactionId,
                       cbData->fragmentId, success);
}

void handleUnloadNanoappCallback(uint16_t /*eventType*/, void *data) {
  auto *cbData = static_cast<UnloadNanoappCallbackData *>(data);
  bool success = false;
  uint32_t instanceId;
  EventLoop &eventLoop = EventLoopManagerSingleton::get()->getEventLoop();
  if (!eventLoop.findNanoappInstanceIdByAppId(cbData->appId, &instanceId)) {
    LOGE("Couldn't unload app ID 0x%016" PRIx64 ": not found", cbData->appId);
  } else {
    success =
        eventLoop.unloadNanoapp(instanceId, cbData->allowSystemNanoappUnload);
  }

  constexpr size_t kInitialBufferSize = 52;
  ChreFlatBufferBuilder builder(kInitialBufferSize);
  HostProtocolChre::encodeUnloadNanoappResponse(builder, cbData->hostClientId,
                                                cbData->transactionId, success);

  if (!getHostCommsManager().send(builder.GetBufferPointer(),
                                  builder.GetSize())) {
    LOGE("Failed to send unload response to host: %x transactionID: 0x%x",
         cbData->hostClientId, cbData->transactionId);
  }

  memoryFree(data);
}

}  // anonymous namespace

void sendDebugDumpResultToHost(uint16_t hostClientId, const char *debugStr,
                               size_t debugStrSize, bool complete,
                               uint32_t dataCount) {
  if (debugStrSize > 0) {
    sendDebugDumpData(hostClientId, debugStr, debugStrSize);
  }

  if (complete) {
    sendDebugDumpResponse(hostClientId, true /*success*/, dataCount);
  }
}

void HostLink::flushMessagesSentByNanoapp(uint64_t /* appId */) {
  // TODO: Implement this once USF supports this (b/149318001)
}

bool HostLink::sendMessage(const MessageToHost *message) {
  constexpr size_t kFixedReserveSize = 80;
  ChreFlatBufferBuilder builder(message->message.size() + kFixedReserveSize);
  HostProtocolChre::encodeNanoappMessage(
      builder, message->appId, message->toHostData.messageType,
      message->toHostData.hostEndpoint, message->message.data(),
      message->message.size());

  bool success =
      getHostCommsManager().send(builder.GetBufferPointer(), builder.GetSize());

  getHostCommsManager().onMessageToHostComplete(message);

  return success;
}

HostLinkBase::HostLinkBase() {
  usf::UsfErr err = usf::UsfTransportMgr::SetMsgHandler(
      usf::UsfMsgType::UsfMsgType_CHRE, handleUsfMessage, nullptr);
  if (err == usf::kErrNone) {
    err = usf::UsfWorkMgr::CreateWorker(&mWorker);
  }

  if (err != usf::kErrNone) {
    FATAL_ERROR("Failed to initialize CHRE transport");
  }
}

HostLinkBase::~HostLinkBase() {
  // Remove the usf message handler to drop any host messages that we might
  // receive during a shutdown.
  usf::UsfTransportMgr::SetMsgHandler(usf::UsfMsgType::UsfMsgType_CHRE,
                                      nullptr /*handler*/,
                                      nullptr /*handler arg*/);
  mWorker->Stop();
  mWorker.reset();
}

void HostLinkBase::init(usf::UsfTransport *transport) {
  if (mTransportHandle == nullptr) {
    if (transport == nullptr) {
      FATAL_ERROR("Null transport at init, cannot send out messages!");
    }
    mTransportHandle = transport;
  }
}

bool HostLinkBase::send(uint8_t *data, size_t dataLen) {
  usf::UsfErr err = usf::kErrFailed;

  if (mTransportHandle != nullptr) {
    usf::UsfTxMsg msg;
    msg.SetMsgType(usf::UsfMsgType_CHRE);
    msg.SetData(data, dataLen);
    err = mTransportHandle->SendMsg(&msg);
  }

  return (usf::kErrNone == err);
}

usf::UsfErr HostLinkBase::handleUsfMessage(usf::UsfTransport *transport,
                                           const usf::UsfMsg *msg,
                                           void * /* arg */) {
  usf::UsfErr rc = usf::kErrFailed;
  getHostCommsManager().init(transport);

  if (msg == nullptr) {
    rc = usf::kErrInvalid;
  } else {
    usf::UsfMsgType msgType = msg->msg_type();

    if (msgType == usf::UsfMsgType_CHRE) {
      auto *dataVector = msg->data();
      if (dataVector != nullptr) {
        size_t dataLen = dataVector->size();
        struct MessageData {
          uint64_t dataLen;
          void *data;
        };

        // handleUsfMessage isn't single-threaded. Make a copy and process
        // on a worker thread to ensure CHRE only processes incoming messages
        // on a single thread. CHRE's thread isn't used since several messages
        // can be processed without posting to it.
        //
        // TODO(b/163592230): Improve USF messaging interface to remove extra
        // data copies and keep work single-threaded.
        MessageData data;
        data.dataLen = dataLen;
        data.data = memoryAlloc(dataLen);
        if (data.data == nullptr) {
          LOG_OOM();
          rc = usf::kErrAllocation;
        } else {
          memcpy(data.data, dataVector->data(), dataLen);

          auto callback = [](void *data) {
            MessageData *dataInfo = static_cast<MessageData *>(data);
            HostProtocolChre::decodeMessageFromHost(dataInfo->data,
                                                    dataInfo->dataLen);
            memoryFree(dataInfo->data);
            // USF owns the passed in data pointer and will free it after the
            // callback returns.
          };
          rc = getHostCommsManager().getWorker()->Enqueue(callback, &data,
                                                          sizeof(data));
        }
      }
    } else {
      rc = usf::kErrInvalid;
    }
  }

  // Opportunistically send a time sync message (1 hour period threshold)
  constexpr Seconds kOpportunisticTimeSyncPeriod = Seconds(60 * 60 * 1);
  if (SystemTime::getMonotonicTime() >
      mLastTimeSyncRequestNanos + kOpportunisticTimeSyncPeriod) {
    sendTimeSyncRequest();
  }

  return rc;
}

void HostLinkBase::sendLogMessage(const uint8_t *logMessage,
                                  size_t logMessageSize) {
  constexpr size_t kInitialSize = 128;
  ChreFlatBufferBuilder builder(logMessageSize + kInitialSize);
  HostProtocolChre::encodeLogMessages(builder, logMessage, logMessageSize);

  getHostCommsManager().send(builder.GetBufferPointer(), builder.GetSize());
}

void HostLinkBase::sendTimeSyncRequest() {
  constexpr size_t kInitialSize = 52;
  ChreFlatBufferBuilder builder(kInitialSize);
  HostProtocolChre::encodeTimeSyncRequest(builder);

  getHostCommsManager().send(builder.GetBufferPointer(), builder.GetSize());

  mLastTimeSyncRequestNanos = SystemTime::getMonotonicTime();
}

void HostMessageHandlers::handleDebugDumpRequest(uint16_t hostClientId) {
  chre::EventLoopManagerSingleton::get()
      ->getDebugDumpManager()
      .onDebugDumpRequested(hostClientId);
}

void HostMessageHandlers::handleHubInfoRequest(uint16_t hostClientId) {
  constexpr size_t kInitialBufferSize = 192;

  constexpr char kHubName[] = "CHRE on AoC";
  constexpr char kVendor[] = "Google";
  constexpr char kToolchain[] =
      "Clang " STRINGIFY(__clang_major__) "." STRINGIFY(
          __clang_minor__) "." STRINGIFY(__clang_patchlevel__) ")";
  constexpr uint32_t kLegacyPlatformVersion = 0;
  constexpr uint32_t kLegacyToolchainVersion =
      ((__clang_major__ & 0xFF) << 24) | ((__clang_minor__ & 0xFF) << 16) |
      (__clang_patchlevel__ & 0xFFFF);
  constexpr float kPeakMips = 800;
  // TODO: The following need to be updated when we've measured the
  // indicated power levels on the AoC
  constexpr float kStoppedPower = 0;
  constexpr float kSleepPower = 0;
  constexpr float kPeakPower = 0;

  ChreFlatBufferBuilder builder(kInitialBufferSize);
  HostProtocolChre::encodeHubInfoResponse(
      builder, kHubName, kVendor, kToolchain, kLegacyPlatformVersion,
      kLegacyToolchainVersion, kPeakMips, kStoppedPower, kSleepPower,
      kPeakPower, CHRE_MESSAGE_TO_HOST_MAX_SIZE, chreGetPlatformId(),
      chreGetVersion(), hostClientId);

  if (!getHostCommsManager().send(builder.GetBufferPointer(),
                                  builder.GetSize())) {
    LOGE("Failed to send Hub Info Response message");
  }
}

void HostMessageHandlers::handleLoadNanoappRequest(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    uint32_t appVersion, uint32_t appFlags, uint32_t targetApiVersion,
    const void *buffer, size_t bufferLen, const char *appFileName,
    uint32_t fragmentId, size_t appBinaryLen) {
  bool success = true;
  static NanoappLoadManager sLoadManager;

  if (fragmentId == 0 || fragmentId == 1) {
    size_t totalAppBinaryLen = (fragmentId == 0) ? bufferLen : appBinaryLen;
    LOGD("Load nanoapp request for app ID 0x%016" PRIx64 " ver 0x%" PRIx32
         " flags 0x%" PRIx32 " target API 0x%08" PRIx32
         " size %zu (txnId %" PRIu32 " client %" PRIu16 ")",
         appId, appVersion, appFlags, targetApiVersion, totalAppBinaryLen,
         transactionId, hostClientId);

    if (sLoadManager.hasPendingLoadTransaction()) {
      FragmentedLoadInfo info = sLoadManager.getTransactionInfo();
      sendFragmentResponse(info.hostClientId, info.transactionId,
                           0 /* fragmentId */, false /* success */);
      sLoadManager.markFailure();
    }

    success =
        sLoadManager.prepareForLoad(hostClientId, transactionId, appId,
                                    appVersion, appFlags, totalAppBinaryLen);
  }

  if (success) {
    success = sLoadManager.copyNanoappFragment(
        hostClientId, transactionId, (fragmentId == 0) ? 1 : fragmentId, buffer,
        bufferLen);
  } else {
    LOGE("Failed to prepare for load");
  }

  if (sLoadManager.isLoadComplete()) {
    LOGD("Load manager load complete...");
    auto cbData = MakeUnique<LoadNanoappCallbackData>();
    if (cbData.isNull()) {
      LOG_OOM();
    } else {
      cbData->transactionId = transactionId;
      cbData->hostClientId = hostClientId;
      cbData->appId = appId;
      cbData->fragmentId = fragmentId;
      cbData->nanoapp = sLoadManager.releaseNanoapp();

      // Note that if this fails, we'll generate the error response in
      // the normal deferred callback
      EventLoopManagerSingleton::get()->deferCallback(
          SystemCallbackType::FinishLoadingNanoapp, cbData.release(),
          finishLoadingNanoappCallback);
    }
  } else {
    // send a response for this fragment
    sendFragmentResponse(hostClientId, transactionId, fragmentId, success);
  }
}

void HostMessageHandlers::handleNanoappListRequest(uint16_t hostClientId) {
  LOGD("Nanoapp list request from client ID %" PRIu16, hostClientId);
  HostClientIdCallbackData cbData = {};
  cbData.hostClientId = hostClientId;
  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::NanoappListResponse, cbData.ptr,
      constructNanoappListCallback);
}

void HostMessageHandlers::handleNanoappMessage(uint64_t appId,
                                               uint32_t messageType,
                                               uint16_t hostEndpoint,
                                               const void *messageData,
                                               size_t messageDataLen) {
  LOGD("Parsed nanoapp message from host: app ID 0x%016" PRIx64
       " endpoint 0x%" PRIx16 " msgType %" PRIu32 " payload size %zu",
       appId, hostEndpoint, messageType, messageDataLen);

  getHostCommsManager().sendMessageToNanoappFromHost(
      appId, messageType, hostEndpoint, messageData, messageDataLen);
}

void HostMessageHandlers::handleSettingChangeMessage(fbs::Setting setting,
                                                     fbs::SettingState state) {
  Setting chreSetting;
  SettingState chreSettingState;
  if (HostProtocolChre::getSettingFromFbs(setting, &chreSetting) &&
      HostProtocolChre::getSettingStateFromFbs(state, &chreSettingState)) {
    postSettingChange(chreSetting, chreSettingState);
  }
}

void HostMessageHandlers::handleTimeSyncMessage(int64_t offset) {
  setEstimatedHostTimeOffset(offset);

  // Schedule a time sync request since offset may drift
  constexpr Seconds kClockDriftTimeSyncPeriod =
      Seconds(60 * 60 * 6);  // 6 hours
  setTimeSyncRequestTimer(kClockDriftTimeSyncPeriod);
}

void HostMessageHandlers::handleUnloadNanoappRequest(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    bool allowSystemNanoappUnload) {
  LOGD("Unload nanoapp request (txnID %" PRIu32 ") for appId 0x%016" PRIx64
       " system %d",
       transactionId, appId, allowSystemNanoappUnload);
  auto *cbData = memoryAlloc<UnloadNanoappCallbackData>();
  if (cbData == nullptr) {
    LOG_OOM();
  } else {
    cbData->appId = appId;
    cbData->transactionId = transactionId;
    cbData->hostClientId = hostClientId;
    cbData->allowSystemNanoappUnload = allowSystemNanoappUnload;

    EventLoopManagerSingleton::get()->deferCallback(
        SystemCallbackType::HandleUnloadNanoapp, cbData,
        handleUnloadNanoappCallback);
  }
}

}  // namespace chre
