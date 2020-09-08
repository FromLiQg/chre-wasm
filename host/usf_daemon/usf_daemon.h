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

#ifndef CHRE_USF_DAEMON_H_
#define CHRE_USF_DAEMON_H_

#include "chre_host/daemon_base.h"
#include "chre_host/socket_server.h"
#ifdef CHRE_USE_TOKENIZED_LOGGING
#include "chre_host/tokenized_log_message_parser.h"
#else
#include "chre_host/log_message_parser_base.h"
#endif

#include "usf/usf_transport_client.h"

#include <utils/SystemClock.h>

// Disable verbose logging
// TODO: use property_get_bool to make verbose logging runtime configurable
// #define LOG_NDEBUG 0

namespace android {
namespace chre {

class UsfChreDaemon : public ChreDaemonBase {
 public:
  struct UsfServerConnection {
    refcount::reffed_ptr<usf::UsfTransportClient> transportClient;
    usf::UsfTransport *transport;
    usf::UsfServerHandle serverHandle;
  };

  ~UsfChreDaemon() {
    deinit();
  }

  /**
   * Initializes the CHRE daemon, and creates a USF transport for
   * communicating with CHRE
   *
   * @return true on successful init
   */
  bool init();

  /**
   * Starts a socket server receive loop for inbound messages
   */
  void run();

  bool sendMessageToChre(uint16_t clientId, void *data, size_t length);

  void onMessageReceived(const unsigned char *messageBuffer, size_t messageLen);

  UsfServerConnection &getMutableServerConnection() {
    return mConnection;
  }

 protected:
  void loadPreloadedNanoapp(const std::string &directory,
                            const std::string &name,
                            uint32_t transactionId) override;

  void handleDaemonMessage(const uint8_t *message) override;

 private:
  SocketServer mSocketServer;
  ChreLogMessageParserBase mLogger;
  UsfServerConnection mConnection;

  //! The mutex used to guard state between the nanoapp messaging thread
  //! and loading preloaded nanoapps.
  std::mutex mPreloadedNanoappsMutex;

  //! The condition variable used to wait for a nanoapp to finish loading.
  std::condition_variable mPreloadedNanoappsCond;

  //! Set to true when a preloaded nanoapp is pending load.
  bool mPreloadedNanoappPending;

  //! Set to the expected transaction ID for loading a nanoapp.
  uint32_t mPreloadedNanoappPendingTransactionId;

  /**
   * Perform a graceful shutdown of the daemon
   */
  void deinit();

  /**
   * Platform specific getTimeOffset for the WHI Daemon
   *
   * @return clock drift offset in nanoseconds
   */
  int64_t getTimeOffset(bool *success);

  /**
   * Get the Log Message Parser configured for this platform
   *
   * @return An instance of a log message parser
   */
  ChreLogMessageParserBase getLogMessageParser();

  /**
   * Loads the supplied file into the provided buffer.
   *
   * @param filename The name of the file to load.
   * @param buffer The buffer to load into.
   * @return true if successful, false otherwise.
   */
  bool readFileContents(const std::string &filename,
                        std::vector<uint8_t> &buffer);

  /**
   * Sends a preloaded nanoapp to CHRE.
   *
   * @param header The nanoapp header binary blob.
   * @param nanoapp The nanoapp binary blob.
   * @param transactionId The transaction ID to use when loading the app.
   * @return true if succssful, false otherwise.
   */
  bool loadNanoapp(const std::vector<uint8_t> &header,
                   const std::vector<uint8_t> &nanoapp, uint32_t transactionId);

  /**
   * Loads a nanoapp using fragments.
   *
   * @param appId The ID of the nanoapp to load.
   * @param appVersion The version of the nanoapp to load.
   * @param appFlags The flags specified by the nanoapp to be loaded.
   * @param appTargetApiVersion The version of the CHRE API that the app
   * targets.
   * @param appBinary The application binary code.
   * @param appSize The size of the appBinary.
   * @param transactionId The transaction ID to use when loading.
   * @return true if successful, false otherwise.
   */
  bool sendFragmentedNanoappLoad(uint64_t appId, uint32_t appVersion,
                                 uint32_t appFlags,
                                 uint32_t appTargetApiVersion,
                                 const uint8_t *appBinary, size_t appSize,
                                 uint32_t transactionId);

  static usf::UsfErr usfMessageHandler(usf::UsfTransport *transport,
                                       const usf::UsfMsg *usfMsg, void *arg);

  usf::UsfErr sendUsfMessage(const uint8_t *data, size_t dataLen);

  bool sendFragmentAndWaitOnResponse(uint32_t transactionId,
                                     flatbuffers::FlatBufferBuilder &builder);
};

}  // namespace chre
}  // namespace android

#endif  // CHRE_USF_DAEMON_H_
