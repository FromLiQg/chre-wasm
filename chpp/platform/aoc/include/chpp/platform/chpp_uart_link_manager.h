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
#include "gpio_aoc.h"
#include "uart.h"

namespace chpp {

/**
 * A class to manage a CHPP link to a remote CHPP endpoint using UART as the
 * physical layer. Each physical link must instantiate its own instance of
 * the UartLinkManager.
 *
 * This class implements the wake handshaking protocol as follows. Each endpoint
 * is expected to have a wake_in and wake_out GPIO and a bidirectional UART
 * link.
 *
 * 1) A transaction begins by a request to transmit a packet from either
 * end of the link. If the local endpoint has data to transmit, it must notify
 * the remote endpoint by asserting its wake_out GPIO. Consequently, a request
 * from the remote endpoint is driven by an IRQ from the wake_in GPIO.
 *
 * 2) If a packet is pending, it can be transmitted once the remote end has
 * asserted its wake_out GPIO. Otherwise, the wake_out GPIO pulse should be
 * asserted for at least the duration to satisfy the pulse width requirements of
 * the specification.
 *
 * 3) The wake_out GPIO can be deasserted once the transmission is complete (if
 * any). No transaction can begin until the current one has completed, and both
 * GPIOs have been deasserted.
 */
class UartLinkManager : public chre::NonCopyable {
 public:
  /**
   * @param uart The pointer to the UART instance.
   * @param wakeOutPinNumber The pin number of the wake_out GPIO.
   */
  UartLinkManager(UART *uart, uint8_t wakeOutPinNumber);

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
   *
   * @return true if the transaction succeeded.
   */
  bool startTransaction();

 private:
  UART *mUart = nullptr;

  GPIOAoC mWakeOutGpio;

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
