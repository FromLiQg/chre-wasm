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

#include "FreeRTOS.h"
#include "chpp/condition_variable.h"
#include "chpp/link.h"
#include "chpp/mutex.h"
#include "chpp/platform/chpp_task_util.h"
#include "chpp/transport.h"
#include "chre/platform/atomic.h"
#include "chre/util/non_copyable.h"
#include "chre/util/time.h"
#include "core_monitor.h"
#include "gpi_aoc.h"
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
   * @param context The context pointer of the CHPP transport.
   * @param uart The pointer to the UART instance.
   * @param wakeOutPinNumber The pin number of the wake_out GPIO.
   * @param wakeInGpiNumber The GPI number of the wake_in GPIO.
   * @param mask The mask to use when preventing monitor mode.
   */
  UartLinkManager(struct ChppTransportState *context, UART *uart,
                  uint8_t wakeOutPinNumber, uint8_t wakeInGpiNumber,
                  enum PreventCoreMonitorMask mask);

  /**
   * This method must be called before invoking the rest of the public methods
   * in this class.
   *
   * @param handle The task handle that this manager will run in.
   */
  void init(TaskHandle_t handle);

  /**
   * Resets the state and disables the UartLinkManager. init() must be called
   * after invoking this method to use this class again.
   */
  void deinit();

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

  /**
   * Pulls RX data from the UART peripheral.
   */
  void pullRxSamples();

  /**
   * Same as pullRxSamples() but also sends the data to the CHPP transport.
   */
  void processRxSamples();

  struct ChppTransportState *getTransportContext() const {
    return mTransportContext;
  }

  UART *getUart() const {
    return mUart;
  }

  GPIAoC *getWakeInGpi() {
    return &mWakeInGpi;
  }

  void setTransactionPending() {
    mTransactionPending = true;
  }

  TaskHandle_t getTaskHandle() const {
    return mTaskHandle;
  }

  /**
   * Waits for a wake handshaking IRQ to trigger, with a specified timeout.
   *
   * @param timeoutNs The timeout in nanoseconds.
   *
   * @return false if timed out
   */
  bool waitForHandshakeIrq(uint64_t timeoutNs);

  bool isRxBufferFull() const {
    return mRxBufIndex == kRxBufSize;
  }

  /**
   * GPIO pin numbers to be used in the argument of UartLinkManager, which
   * defines the physical pin used for the wake_out GPIO.
   */
  static constexpr uint8_t kWifiWakeOutGpioPinNumber = 43;
  static constexpr uint8_t kGnssWakeOutGpioPinNumber = 86;
  static constexpr uint8_t kWwanWakeOutGpioPinNumber = 84;

  /**
   * GPI numbers to be used in the argument of UartLinkManager, which defines
   * the GPI used to generate the wake_in interrupts.
   */
  static constexpr uint8_t kWifiWakeInGpiNumber = 46;
  static constexpr uint8_t kGnssWakeInGpiNumber = 44;
  static constexpr uint8_t kWwanWakeInGpiNumber = 42;

 private:
  TaskHandle_t mTaskHandle = nullptr;

  struct ChppTransportState *mTransportContext = nullptr;

  UART *mUart = nullptr;

  GPIOAoC mWakeOutGpio;

  GPIAoC mWakeInGpi;

  //! The pointer to the currently pending TX packet.
  uint8_t *mCurrentBuffer = nullptr;
  size_t mCurrentBufferLen = 0;

  //! The temporary buffer to store RX data.
  //! NOTE: Access to these variables must be done when the RX interrupt is
  //! disabled.
  static constexpr size_t kRxBufSize = CHPP_PLATFORM_LINK_RX_MTU_BYTES;
  volatile uint8_t mRxBuf[kRxBufSize];
  volatile size_t mRxBufIndex = 0;

  //! The timeout for waiting on the remote to indicate transaction readiness
  //! (t_start per specifications).
  static constexpr uint64_t kStartTimeoutNs =
      100 * chre::kOneMillisecondInNanoseconds;

  //! The timeout for waiting on the remote to indicate transaction ended
  //! (t_end per specifications).
  static constexpr uint64_t kEndTimeoutNs = chre::kOneSecondInNanoseconds;

  //! The minimum amount of time to assert the wake_out GPIO (t_pulse per
  //! specifications).
  static constexpr uint64_t kPulseTimeNs =
      100 * chre::kOneMicrosecondInNanoseconds;

  chre::AtomicBool mTransactionPending{false};

  TaskUtil mTaskUtil;

  // The mask to use when allowing or preventing core monitor mode. This should
  // be used while wake handshaking is in progress to avoid the UART clock
  // from being turned off.
  enum PreventCoreMonitorMask mCoreMonitorMask;

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
