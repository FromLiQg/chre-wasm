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

#ifndef CHRE_PLATFORM_AOC_HOST_LINK_BASE_H_
#define CHRE_PLATFORM_AOC_HOST_LINK_BASE_H_

#include "chre/util/time.h"

#include "usf/error.h"
#include "usf/usf_transport.h"
#include "usf/usf_work.h"

namespace chre {

/**
 * Helper function to send debug dump result to host.
 */
void sendDebugDumpResultToHost(uint16_t hostClientId, const char *debugStr,
                               size_t debugStrSize, bool complete,
                               uint32_t dataCount);

/**
 * This class implements a basic wrapper for sending/receiving messages over USF
 * transports. It sets up a callback into the USF transport layer for receiving
 * messages on instantiation. The first message that we receive from the USF
 * transport layer includes a pointer to a 'transport' abstraction, that can be
 * used to send messages out of CHRE.
 *
 * TODO: Revisit this implementation after the USF feature requests in
 * b/149318001 and b/149317051 are fulfilled
 */
class HostLinkBase {
 public:
  HostLinkBase();
  ~HostLinkBase();

  /**
   * Initializes the host link with the transport provided when the CHRE daemon
   * sends its first message to AoC. If called multiple times, only the first
   * transport is used.
   *
   * TODO (b/149317051) Remove this once USF exposes an API to acquire a
   * transport for communicating with the AP.
   */
  void init(usf::UsfTransport *transport);

  /**
   * Sends a message to the CHRE daemon.
   *
   * @param data data to be sent to the daemon
   * @param dataLen length of the data being sent
   * @return true if the data was successfully queued for sending
   */
  bool send(uint8_t *data, size_t dataLen);

  refcount::reffed_ptr<usf::UsfWorker> &getWorker() {
    return mWorker;
  }

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
  static usf::UsfErr handleUsfMessage(usf::UsfTransport *transport,
                                      const usf::UsfMsg *msg, void *arg);

  /**
   * Sends a request to the host for a time sync message.
   */
  static void sendTimeSyncRequest();

  /**
   * Enqueues a log message to be sent to the host.
   *
   * @param logMessage Pointer to a buffer that has the log message. Note that
   * the message might be encoded
   *
   * @param logMessageSize length of the log message buffer
   */
  void sendLogMessage(const uint8_t *logMessage, size_t logMessageSize);

 private:
  //! The last time a time sync request message has been sent.
  static Nanoseconds mLastTimeSyncRequestNanos;

  //! Transport handle provided when the CHRE daemon sends its first message
  //! to AoC.
  usf::UsfTransport *mTransportHandle = nullptr;

  //! Worker thread used to ensure single-threaded communication with CHRE.
  refcount::reffed_ptr<usf::UsfWorker> mWorker;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_AOC_HOST_LINK_BASE_H_
