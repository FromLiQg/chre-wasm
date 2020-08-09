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
#include "chre/platform/shared/dram_util.h"
#include "chre/platform/shared/host_protocol_chre.h"
#include "chre/platform/shared/nanoapp_load_manager.h"
#include "chre/util/fixed_size_blocking_queue.h"
#include "chre_api/chre/version.h"

#include "usf/usf_transport.h"

using flatbuffers::FlatBufferBuilder;

namespace chre {

namespace {

//! The last time a time sync request message has been sent.
//! TODO: Make this a member of HostLinkBase
Nanoseconds gLastTimeSyncRequestNanos(0);

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

/**
 * Sends a request to the host for a time sync message.
 */
void sendTimeSyncRequest();

// Forward declare the USF external message handler
usf::UsfErr handleUsfMessage(usf::UsfTransport *transport,
                             const usf::UsfMsg *msg, void *arg);

// This class implements a basic wrapper for sending/receiving messages
// over USF transports. It sets up a callback into the USF transport layer
// for receiving messages on instantiation. The first message that we
// receive from the USF transport layer includes a pointer to a 'transport'
// abstraction, that can be used to send messages out of CHRE.
// TODO: Revisit this implementation after the USF feature requests in
// b/149318001 and b/149317051 are fulfilled
class ChreUsfTransportManager {
 public:
  ChreUsfTransportManager() : mInitialized(false), mTransportHandle(nullptr) {
    usf::UsfErr err = usf::UsfTransportMgr::SetMsgHandler(
        usf::UsfMsgType::UsfMsgType_CHRE, handleUsfMessage, nullptr);
    if (usf::kErrNone != err) {
      FATAL_ERROR("Failed to set USF msg handler");
    }
  }

  // Check if we've already received a message from the AP, since that's the
  // only way we can send outgoing messages.
  // AP (transport is valid).
  // TODO (b/149317051) This needs to go away once USF exposes an API to
  // acquire a transport.
  void checkInit(usf::UsfTransport *transport) {
    if (!mInitialized) {
      if (transport == nullptr) {
        FATAL_ERROR("Null transport at init, cannot send out messages!");
      }
      mTransportHandle = transport;
      mInitialized = true;
    }
  }

  bool send(uint8_t *data, size_t dataLen) {
    usf::UsfErr err = usf::kErrFailed;

    if (mInitialized) {
      usf::UsfTxMsg msg;
      msg.SetMsgType(usf::UsfMsgType_CHRE);
      msg.SetData(data, dataLen);
      err = mTransportHandle->SendMsg(&msg);
    }

    return (usf::kErrNone == err);
  }

 private:
  bool mInitialized;
  usf::UsfTransport *mTransportHandle;
};

ChreUsfTransportManager transportMgr;

/**
 * The USF external message handler function for CHRE, that
 * is invoked on CHRE inbound messages from the AP
 *
 * @param transport A pointer to a transport that is used to send messages
 * out of CHRE to the AP
 * @param msg flatbuffer encoded message of type UsfMsgType_CHRE
 * @param arg yields the message length when cast to size_t
 * @return usf::kErrNone if success, an error code indicating the failure
 * otherwise
 */
usf::UsfErr handleUsfMessage(usf::UsfTransport *transport,
                             const usf::UsfMsg *msg, void * /* arg */) {
  usf::UsfErr rc = usf::kErrFailed;

  // check for initialization if this is the first message received
  transportMgr.checkInit(transport);

  if (msg == nullptr) {
    return usf::kErrInvalid;

  } else {
    usf::UsfMsgType msgType = msg->msg_type();

    if (msgType == usf::UsfMsgType_CHRE) {
      auto *dataVector = msg->data();
      if (dataVector != nullptr) {
        size_t dataLen = dataVector->size();
        bool success = HostProtocolChre::decodeMessageFromHost(
            dataVector->data(), dataLen);
        rc = (success == true) ? usf::kErrNone : usf::kErrFailed;
      }
    } else {
      rc = usf::kErrInvalid;
    }
  }

  // Opportunistically send a time sync message (1 hour period threshold)
  constexpr Seconds kOpportunisticTimeSyncPeriod = Seconds(60 * 60 * 1);
  if (SystemTime::getMonotonicTime() >
      gLastTimeSyncRequestNanos + kOpportunisticTimeSyncPeriod) {
    sendTimeSyncRequest();
  }

  return rc;
}

void sendTimeSyncRequest() {
  constexpr size_t kInitialSize = 52;
  flatbuffers::FlatBufferBuilder builder(kInitialSize);
  HostProtocolChre::encodeTimeSyncRequest(builder);

  transportMgr.send(builder.GetBufferPointer(), builder.GetSize());

  gLastTimeSyncRequestNanos = SystemTime::getMonotonicTime();
}

void setTimeSyncRequestTimer(Nanoseconds delay) {
  static TimerHandle sHandle;
  static bool sHandleInitialized;

  if (sHandleInitialized) {
    EventLoopManagerSingleton::get()->cancelDelayedCallback(sHandle);
  }

  auto callback = [](uint16_t /* eventType */, void * /* data */) {
    sendTimeSyncRequest();
  };
  sHandle = EventLoopManagerSingleton::get()->setDelayedCallback(
      SystemCallbackType::TimerSyncRequest, nullptr /* data */, callback,
      delay);
  sHandleInitialized = true;
}

bool getSettingFromFbs(fbs::Setting setting, Setting *chreSetting) {
  bool success = true;
  switch (setting) {
    case fbs::Setting::LOCATION:
      *chreSetting = Setting::LOCATION;
      break;
    default:
      LOGE("Unknown setting %" PRIu8, setting);
      success = false;
  }

  return success;
}

bool getSettingStateFromFbs(fbs::SettingState state,
                            SettingState *chreSettingState) {
  bool success = true;
  switch (state) {
    case fbs::SettingState::DISABLED:
      *chreSettingState = SettingState::DISABLED;
      break;
    case fbs::SettingState::ENABLED:
      *chreSettingState = SettingState::ENABLED;
      break;
    default:
      LOGE("Unknown state %" PRIu8, state);
      success = false;
  }

  return success;
}

void sendDebugDumpData(uint16_t hostClientId, const char *debugStr,
                       size_t debugStrSize) {
  constexpr size_t kFixedSizePortion = 52;
  flatbuffers::FlatBufferBuilder builder(kFixedSizePortion + debugStrSize);
  HostProtocolChre::encodeDebugDumpData(builder, hostClientId, debugStr,
                                        debugStrSize);

  transportMgr.send(builder.GetBufferPointer(), builder.GetSize());
}

void sendDebugDumpResponse(uint16_t hostClientId, bool success,
                           uint32_t dataCount) {
  constexpr size_t kFixedSizePortion = 52;
  flatbuffers::FlatBufferBuilder builder(kFixedSizePortion);
  HostProtocolChre::encodeDebugDumpResponse(builder, hostClientId, success,
                                            dataCount);
  transportMgr.send(builder.GetBufferPointer(), builder.GetSize());
}

void constructNanoappListCallback(uint16_t /*eventType*/, void *deferCbData) {
  HostClientIdCallbackData clientIdCbData;
  clientIdCbData.ptr = deferCbData;

  struct NanoappListData {
    FlatBufferBuilder *builder;
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

    flatbuffers::FlatBufferBuilder builder(initialBufferSize);
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
    transportMgr.send(builder.GetBufferPointer(), builder.GetSize());
  }
}

void sendFragmentResponse(uint16_t hostClientId, uint32_t transactionId,
                          uint32_t fragmentId, bool success) {
  constexpr size_t kInitialBufferSize = 52;
  flatbuffers::FlatBufferBuilder builder(kInitialBufferSize);
  HostProtocolChre::encodeLoadNanoappResponse(
      builder, hostClientId, transactionId, success, fragmentId);

  if (!transportMgr.send(builder.GetBufferPointer(), builder.GetSize())) {
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
  flatbuffers::FlatBufferBuilder builder(kInitialBufferSize);

  CHRE_ASSERT(cbData != nullptr);
  LOGD("Finished loading nanoapp cb on fragment ID: %u", cbData->fragmentId);

  EventLoop &eventLoop = EventLoopManagerSingleton::get()->getEventLoop();
  bool success =
      cbData->nanoapp->isLoaded() && eventLoop.startNanoapp(cbData->nanoapp);

  sendFragmentResponse(cbData->hostClientId, cbData->transactionId,
                       cbData->fragmentId, success);
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
  flatbuffers::FlatBufferBuilder builder(message->message.size() +
                                         kFixedReserveSize);
  HostProtocolChre::encodeNanoappMessage(
      builder, message->appId, message->toHostData.messageType,
      message->toHostData.hostEndpoint, message->message.data(),
      message->message.size());

  return transportMgr.send(builder.GetBufferPointer(), builder.GetSize());
}

void HostLink::sendLogMessage(const char *logMessage, size_t logMessageSize) {
  constexpr size_t kInitialSize = 128;
  flatbuffers::FlatBufferBuilder builder(logMessageSize + kInitialSize);
  HostProtocolChre::encodeLogMessages(builder, logMessage, logMessageSize);

  transportMgr.send(builder.GetBufferPointer(), builder.GetSize());
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

  flatbuffers::FlatBufferBuilder builder(kInitialBufferSize);
  HostProtocolChre::encodeHubInfoResponse(
      builder, kHubName, kVendor, kToolchain, kLegacyPlatformVersion,
      kLegacyToolchainVersion, kPeakMips, kStoppedPower, kSleepPower,
      kPeakPower, CHRE_MESSAGE_TO_HOST_MAX_SIZE, chreGetPlatformId(),
      chreGetVersion(), hostClientId);

  if (!transportMgr.send(builder.GetBufferPointer(), builder.GetSize())) {
    LOGE("Failed to send Hub Info Response message");
  }
}

void HostMessageHandlers::handleLoadNanoappRequest(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    uint32_t appVersion, uint32_t targetApiVersion, const void *buffer,
    size_t bufferLen, const char *appFileName, uint32_t fragmentId,
    size_t appBinaryLen) {
  bool success = true;
  static NanoappLoadManager sLoadManager;

  if (fragmentId == 0 || fragmentId == 1) {
    size_t totalAppBinaryLen = (fragmentId == 0) ? bufferLen : appBinaryLen;
    LOGD("Load nanoapp request for app ID 0x%016" PRIx64 " ver 0x%" PRIx32
         " target API 0x%08" PRIx32 " size %zu (txnId %" PRIu32
         " client %" PRIu16 ")",
         appId, appVersion, targetApiVersion, totalAppBinaryLen, transactionId,
         hostClientId);

    if (sLoadManager.hasPendingLoadTransaction()) {
      FragmentedLoadInfo info = sLoadManager.getTransactionInfo();
      sendFragmentResponse(info.hostClientId, info.transactionId,
                           0 /* fragmentId */, false /* success */);
      sLoadManager.markFailure();
    }

    success = sLoadManager.prepareForLoad(hostClientId, transactionId, appId,
                                          appVersion, totalAppBinaryLen);
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

  HostCommsManager &hostCommsManager =
      EventLoopManagerSingleton::get()->getHostCommsManager();
  hostCommsManager.sendMessageToNanoappFromHost(
      appId, messageType, hostEndpoint, messageData, messageDataLen);
}

void HostMessageHandlers::handleSettingChangeMessage(fbs::Setting setting,
                                                     fbs::SettingState state) {
  Setting chreSetting;
  SettingState chreSettingState;
  if (getSettingFromFbs(setting, &chreSetting) &&
      getSettingStateFromFbs(state, &chreSettingState)) {
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
    uint16_t /* hostClientId */, uint32_t /* transactionId */,
    uint64_t /* appId */, bool /* allowSystemNanoappUnload */) {
  // TODO: stubbed out, needs to be implemented
  LOGI("UnloadNanoappRequest messages are currently unsupported");
}

}  // namespace chre
