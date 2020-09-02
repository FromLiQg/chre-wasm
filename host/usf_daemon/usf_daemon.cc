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

#include "usf_daemon.h"

#include <unistd.h>
#include <chrono>
#include <fstream>
#include <thread>

#include "usf/usf_android_time.h"
#include "usf/usf_client.h"

// Aliased for consistency with the way these symbols are referenced in
// CHRE-side code
namespace fbs = ::chre::fbs;

namespace android {
namespace chre {

namespace {

using ReffedTransportClientPtr = refcount::reffed_ptr<usf::UsfTransportClient>;

constexpr uint8_t kConnectRetries = 5;
constexpr int kRetryDelayMilliSeconds = 100;

bool connectToUsfWithRetry(ReffedTransportClientPtr &client) {
  // client has already been nullptr-checked
  uint8_t retryCount = 0;
  bool success = false;
  usf::UsfErr err;

  while (retryCount < kConnectRetries) {
    if ((err = client->Connect()) == usf::kErrNone) {
      success = true;
      break;
    }
    ++retryCount;
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kRetryDelayMilliSeconds));
  }

  if (success) {
    LOGD("Connected to USF Transport server (retry count: %u)", retryCount + 1);
  }

  return success;
}

}  // namespace

bool UsfChreDaemon::init() {
  constexpr size_t kMaxTimeSyncRetries = 5;
  constexpr useconds_t kTimeSyncRetryDelayUs = 50000;  // 50 ms
  bool success = false;
  usf::UsfErr err;

  if ((err = usf::UsfClientMgr::Init()) != usf::kErrNone) {
    LOGE("Failed to initialize usf client mgr: (err %d)",
         static_cast<int>(err));
  } else if ((err = usf::UsfTransportClient::Create(
                  &mConnection.transportClient)) != usf::kErrNone) {
    LOGE("Failed to create a USF transport client: (err %d)",
         static_cast<int>(err));
  } else if (mConnection.transportClient == nullptr) {
    LOGE("Init Failed: Created a NULL transport client!");
  } else if (!connectToUsfWithRetry(mConnection.transportClient)) {
    LOGE("Failed to  connect to USF transport server on %u retries",
         kConnectRetries);
  } else if ((err = usf::UsfTransportMgr::SetMsgHandler(
                  usf::UsfMsgType_CHRE, UsfChreDaemon::usfMessageHandler,
                  this)) != usf::kErrNone) {
    LOGE("Failed to set Usf Message Handler");
  } else if (!sendTimeSyncWithRetry(kMaxTimeSyncRetries, kTimeSyncRetryDelayUs,
                                    true /* logOnError */)) {
    LOGE("Failed to send initial time sync message");
  } else {
    mLogger = getLogMessageParser();
    loadPreloadedNanoapps();
    success = true;
    LOGD("CHRE daemon initialized successfully");
  }

  return success;
}

void UsfChreDaemon::deinit() {
  setShutdownRequested(true);
  usf::UsfClientMgr::Deinit();
}

void UsfChreDaemon::run() {
  constexpr char kChreSocketName[] = "chre";
  auto serverCb = [&](uint16_t clientId, void *data, size_t len) {
    sendMessageToChre(clientId, data, len);
  };

  mSocketServer.run(kChreSocketName, true /* allowSocketCreation */, serverCb);
}

usf::UsfErr UsfChreDaemon::sendUsfMessage(const uint8_t *data, size_t dataLen) {
  usf::UsfTxMsg msg;

  msg.SetMsgType(usf::UsfMsgType_CHRE);
  msg.SetData(data, dataLen);

  return mConnection.transportClient->SendMsg(&msg);
}

bool UsfChreDaemon::sendMessageToChre(uint16_t clientId, void *data,
                                      size_t length) {
  bool success = false;
  usf::UsfErr err;

  if (!HostProtocolHost::mutateHostClientId(data, length, clientId)) {
    LOGE("Couldn't set host client ID in message container!");
  } else {
    LOGV("Delivering message from host (size %zu)", length);
    if ((err = sendUsfMessage(static_cast<const uint8_t *>(data), length)) !=
        usf::kErrNone) {
      LOGE("Failed to deliver message from host to CHRE: %d",
           static_cast<int>(err));
    } else {
      success = true;
    }
  }

  return success;
}

// TODO(karthikmb): Consider moving platform independent parts of this
// function to the base class, revisit when implementing the daemon for
// another platform.
void UsfChreDaemon::onMessageReceived(const unsigned char *messageBuffer,
                                      size_t messageLen) {
  mLogger.dump(messageBuffer, messageLen);

  uint16_t hostClientId;
  fbs::ChreMessage messageType;
  if (!HostProtocolHost::extractHostClientIdAndType(
          messageBuffer, messageLen, &hostClientId, &messageType)) {
    LOGW(
        "Failed to extract host client ID from message - sending "
        "broadcast");
    hostClientId = ::chre::kHostClientIdUnspecified;
  }

  if (messageType == fbs::ChreMessage::LogMessage) {
    std::unique_ptr<fbs::MessageContainerT> container =
        fbs::UnPackMessageContainer(messageBuffer);
    const auto *logMessage = container->message.AsLogMessage();
    const std::vector<int8_t> &logData = logMessage->buffer;

    mLogger.log(reinterpret_cast<const uint8_t *>(logData.data()),
                logData.size());
  } else if (messageType == fbs::ChreMessage::TimeSyncRequest) {
    sendTimeSync(true /* logOnError */);
  } else if (messageType == fbs::ChreMessage::LowPowerMicAccessRequest) {
    LOGD("LpmaRequest unsupported");
  } else if (messageType == fbs::ChreMessage::LowPowerMicAccessRelease) {
    LOGD("LpmaRelease unsupported");
  } else if (hostClientId == kHostClientIdDaemon) {
    handleDaemonMessage(messageBuffer);
  } else if (hostClientId == ::chre::kHostClientIdUnspecified) {
    mSocketServer.sendToAllClients(messageBuffer,
                                   static_cast<size_t>(messageLen));
  } else {
    mSocketServer.sendToClientById(
        messageBuffer, static_cast<size_t>(messageLen), hostClientId);
  }
}

void UsfChreDaemon::handleDaemonMessage(const uint8_t *message) {
  std::unique_ptr<fbs::MessageContainerT> container =
      fbs::UnPackMessageContainer(message);
  if (container->message.type != fbs::ChreMessage::LoadNanoappResponse) {
    LOGE("Invalid message from CHRE directed to daemon");
  } else {
    const auto *response = container->message.AsLoadNanoappResponse();
    std::unique_lock<std::mutex> lock(mPreloadedNanoappsMutex);

    if (!mPreloadedNanoappPending) {
      LOGE("Received nanoapp load response with no pending load");
    } else if (mPreloadedNanoappPendingTransactionId !=
               response->transaction_id) {
      LOGE("Received nanoapp load response with invalid transaction id");
    } else {
      mPreloadedNanoappPending = false;
    }

    mPreloadedNanoappsCond.notify_all();
  }
}

void UsfChreDaemon::loadPreloadedNanoapp(const std::string &directory,
                                         const std::string &name,
                                         uint32_t transactionId) {
  std::vector<uint8_t> headerBuffer;
  std::vector<uint8_t> nanoappBuffer;

  std::string headerFilename = directory + "/" + name + ".napp_header";
  std::string nanoappFilename = directory + "/" + name + ".so";

  if (readFileContents(headerFilename, headerBuffer) &&
      readFileContents(nanoappFilename, nanoappBuffer) &&
      !loadNanoapp(headerBuffer, nanoappBuffer, transactionId)) {
    LOGE("Failed to load nanoapp: '%s'", name.c_str());
  }
}

bool UsfChreDaemon::readFileContents(const std::string &filename,
                                     std::vector<uint8_t> &buffer) {
  bool success = false;
  std::ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
  if (!file) {
    LOGE("Couldn't open file '%s': %d (%s)", filename.c_str(), errno,
         strerror(errno));
  } else {
    ssize_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
      LOGE("Couldn't read from file '%s': %d (%s)", filename.c_str(), errno,
           strerror(errno));
    } else {
      success = true;
    }
  }

  return success;
}

bool UsfChreDaemon::loadNanoapp(const std::vector<uint8_t> &header,
                                const std::vector<uint8_t> &nanoapp,
                                uint32_t transactionId) {
  // This struct comes from build/build_template.mk and must not be modified.
  // Refer to that file for more details.
  struct NanoAppBinaryHeader {
    uint32_t headerVersion;
    uint32_t magic;
    uint64_t appId;
    uint32_t appVersion;
    uint32_t flags;
    uint64_t hwHubType;
    uint8_t targetChreApiMajorVersion;
    uint8_t targetChreApiMinorVersion;
    uint8_t reserved[6];
  } __attribute__((packed));

  bool success = false;
  if (header.size() != sizeof(NanoAppBinaryHeader)) {
    LOGE("Header size mismatch");
  } else {
    // The header blob contains the struct above.
    const auto *appHeader =
        reinterpret_cast<const NanoAppBinaryHeader *>(header.data());

    // Build the target API version from major and minor.
    uint32_t targetApiVersion = (appHeader->targetChreApiMajorVersion << 24) |
                                (appHeader->targetChreApiMinorVersion << 16);

    success = sendFragmentedNanoappLoad(
        appHeader->appId, appHeader->appVersion, appHeader->flags,
        targetApiVersion, nanoapp.data(), nanoapp.size(), transactionId);
  }

  return success;
}

bool UsfChreDaemon::sendFragmentAndWaitOnResponse(
    uint32_t transactionId, flatbuffers::FlatBufferBuilder &builder) {
  bool success = true;
  std::unique_lock<std::mutex> lock(mPreloadedNanoappsMutex);

  mPreloadedNanoappPendingTransactionId = transactionId;
  mPreloadedNanoappPending = sendMessageToChre(
      kHostClientIdDaemon, builder.GetBufferPointer(), builder.GetSize());
  if (!mPreloadedNanoappPending) {
    LOGE("Failed to send nanoapp fragment");
    success = false;
  } else {
    // 60s timeout for fragments. Set high for busy first bootups where the
    // PALs can block CHRE initialization while other subsystems come up.
    std::chrono::seconds timeout(60);
    bool signaled = mPreloadedNanoappsCond.wait_for(
        lock, timeout, [this] { return !mPreloadedNanoappPending; });

    if (!signaled) {
      LOGE("Nanoapp fragment load timed out");
      success = false;
    }
  }
  return success;
}

bool UsfChreDaemon::sendFragmentedNanoappLoad(
    uint64_t appId, uint32_t appVersion, uint32_t appFlags,
    uint32_t appTargetApiVersion, const uint8_t *appBinary, size_t appSize,
    uint32_t transactionId) {
  // TODO: This is currently limited by the USF Message size, revisit
  // and increase this when the USF message size increases.
  constexpr size_t kFragmentSize = 512;
  std::vector<uint8_t> binary(appSize);
  std::copy(appBinary, appBinary + appSize, binary.begin());

  FragmentedLoadTransaction transaction(transactionId, appId, appVersion,
                                        appFlags, appTargetApiVersion, binary,
                                        kFragmentSize);

  bool success = true;

  while (success && !transaction.isComplete()) {
    // Pad the builder to avoid allocation churn.
    const auto &fragment = transaction.getNextRequest();
    flatbuffers::FlatBufferBuilder builder(fragment.binary.size() + 128);
    HostProtocolHost::encodeFragmentedLoadNanoappRequest(builder, fragment);
    success = sendFragmentAndWaitOnResponse(transactionId, builder);
  }

  return success;
}

int64_t UsfChreDaemon::getTimeOffset(bool *success) {
  int64_t offset = 0;
  uint64_t androidTimeNs;
  uint64_t sensorCoreTimeNs;
  usf::UsfErr err =
      usf::UsfGetAndroidAndSensorCoreTime(&androidTimeNs, &sensorCoreTimeNs);
  if (err != usf::kErrNone) {
    LOGE("Get Android and sensor core time failed.");
  } else {
    offset = static_cast<int64_t>(androidTimeNs) -
             static_cast<int64_t>(sensorCoreTimeNs);
  }
  *success = (err == usf::kErrNone);
  return offset;
}

ChreLogMessageParserBase UsfChreDaemon::getLogMessageParser() {
#ifdef CHRE_USE_TOKENIZED_LOGGING
  return ChreTokenizedLogMessageParser();
#else
  return ChreLogMessageParserBase();
#endif
}

usf::UsfErr UsfChreDaemon::usfMessageHandler(
    usf::UsfTransport * /* transport */, const usf::UsfMsg *usfMsg, void *arg) {
  usf::UsfErr err = usf::kErrInvalid;
  auto *daemon = static_cast<UsfChreDaemon *>(arg);
  if ((daemon != nullptr) && (usfMsg != nullptr)) {
    const uint8_t *msgData = usfMsg->data()->data();
    size_t msgLen = usfMsg->data()->size();
    daemon->onMessageReceived(msgData, msgLen);
  } else {
    LOGE("Error Handling Usf Message");
  }
  return err;
}

}  // namespace chre
}  // namespace android
