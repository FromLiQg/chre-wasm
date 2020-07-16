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

#ifndef CHPP_PLATFORM_UART_LINK_MANAGER_H_
#define CHPP_PLATFORM_UART_LINK_MANAGER_H_

#include "chpp/link.h"
#include "chre/util/non_copyable.h"
#include "uart.h"

namespace chpp {

/**
 * A class to manage a CHPP link to a remote CHPP endpoint using UART as the
 * physical layer. Each physical link must instantiate its own instance of
 * the UartLinkManager.
 *
 * TODO: Add details of the wake handshaking protocol this class implements.
 */
class UartLinkManager : public chre::NonCopyable {
 public:
  /**
   * @param params The link parameters supplied to the CHPP link layer.
   */
  UartLinkManager(ChppPlatformLinkParameters *params);

  /**
   * @param buf The non-null pointer to the buffer.
   * @param len The length of the above buffer.
   *
   * @return true if the packet was successfully prepared to be transmitted in
   * the next transaction.
   */
  bool prepareTxPacket(uint8_t *buf, size_t len);

  /**
   * Starts a transaction to transmit any pending packets (if any, via a
   * previous call to prepareTxPacket()), and handles the wake handshaking
   * protocol.
   */
  void startTransaction();

 private:
  UART *mUart = nullptr;

  //! The pointer to the currently pending TX packet.
  uint8_t *mCurrentBuffer = nullptr;
  size_t mCurrentBufferLen = 0;

  /**
   * @return if a TX packet is pending transmission.
   */
  bool hasTxPacket() const;

  /**
   * Clears a pending TX packet.
   */
  void clearTxPacket();
};

}  // namespace chpp

#endif  // CHPP_PLATFORM_UART_LINK_MANAGER_H_
