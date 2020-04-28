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

/**
 * @file
 * The daemon that hosts CHRE on a hexagon DSP via FastRPC. This is typically
 * the SLPI but could be the ADSP or another DSP that supports FastRPC.
 */

#ifndef CHRE_FASTRPC_DAEMON_H_
#define CHRE_FASTRPC_DAEMON_H_

#include "chre/platform/slpi/fastrpc.h"
#include "chre_host/daemon_base.h"
#include "chre_host/socket_server.h"
#include "chre_host/st_hal_lpma_handler.h"

#include "generated/chre_slpi.h"

#include <utils/SystemClock.h>

// Disable verbose logging
// TODO: use property_get_bool to make verbose logging runtime configurable
// #define LOG_NDEBUG 0

#ifdef CHRE_USE_TOKENIZED_LOGGING
#include "chre_host/tokenized_log_message_parser.h"
#else
#include "chre_host/log_message_parser_base.h"
#endif

// TODO: The following conditional compilation needs to be removed, and done
// for all platforms after verifying that it works on older devices where
// we're currently not defining this macro
#ifdef CHRE_DAEMON_LOAD_INTO_SENSORSPD
#include "remote.h"

#define ITRANSPORT_PREFIX "'\":;./\\"
#endif  // CHRE_DAEMON_LOAD_INTO_SENSORSPD

namespace android {
namespace chre {

class FastRpcChreDaemon : public ChreDaemonBase {
 public:
  FastRpcChreDaemon();

  ~FastRpcChreDaemon() {
    deinit();
  }

  /**
   * Initializes and starts the monitoring and message handling threads,
   * then proceeds to load any preloaded nanoapps. Also starts LPMA if
   * it's enabled.
   *
   * @return true on successful init
   */
  bool init();

  /**
   * Starts a socket server receive loop for inbound messages.
   */
  void run();

  bool sendMessageToChre(uint16_t clientId, void *data, size_t length);

  void onMessageReceived(unsigned char *messageBuffer, size_t messageLen);

 private:
  std::optional<std::thread> mMonitorThread;
  std::optional<std::thread> mMsgToHostThread;
  SocketServer mServer;
  ChreLogMessageParserBase mLogger;
  StHalLpmaHandler mLpmaHandler;

  /**
   * Shutsdown the daemon, stops all the worker threads created in init()
   * Since this is to be invoked at exit, it's mostly best effort, and is
   * invoked by the class destructor
   */
  void deinit();

  /**
   * Platform specific getTimeOffset for the FastRPC daemon
   *
   * @return clock drift offset in nanoseconds
   */
  int64_t getTimeOffset(bool *success);

  /**
   * Entry point for the thread that blocks in a FastRPC call to monitor for
   * abnormal exit of CHRE or reboot of the DSP.
   */
  void monitorThreadEntry();

  /**
   * Entry point for the thread that receives messages sent by CHRE.
   */
  void msgToHostThreadEntry();

  /**
   * Get the Log Message Parser configured for this platform
   *
   * @return An instance of a log message parser
   */
  ChreLogMessageParserBase getLogMessageParser();
};

}  // namespace chre
}  // namespace android

#endif  // CHRE_FASTRPC_DAEMON_H_
