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

#include "chpp/platform/chpp_uart_link_manager.h"

#include "aoc.h"
#include "chpp/macros.h"
#include "chpp/platform/log.h"
#include "chre/platform/fatal_error.h"
#include "efw/include/interrupt_controller.h"
#include "efw/include/processor.h"
#include "efw/include/timer.h"
#include "ipc-regions.h"

// Define as 0 to disable wake handshaking
// TODO: Enable once ready
#define WAKE_HANDSHAKE_ENABLE 0

namespace chpp {

namespace {

void onUartRxInterrupt(void *context) {
  UartLinkManager *manager = static_cast<UartLinkManager *>(context);
  manager->getUart()->DisableRxInterrupt();
  manager->pullRxSamples();
  chppWorkThreadSignalFromLink(&manager->getTransportContext()->linkParams,
                               CHPP_TRANSPORT_SIGNAL_LINK_RX_PROCESS);
  if (!manager->isRxBufferFull()) {
    manager->getUart()->EnableRxInterrupt();
  }
}

//! This is the interrupt to use when handling requests from the remote
//! to start a transaction.
void onTransactionRequestInterrupt(void *context, uint32_t /* interrupt */) {
  UartLinkManager *manager = static_cast<UartLinkManager *>(context);
  manager->getWakeInGpi()->SetTriggerFunction(GPIAoC::GPI_DISABLE);
  manager->getWakeInGpi()->ClearInterrupt();
  manager->setTransactionPending();
  chppWorkThreadSignalFromLink(&manager->getTransportContext()->linkParams,
                               CHPP_TRANSPORT_SIGNAL_LINK_WAKE_IN_IRQ);
}

#if WAKE_HANDSHAKE_ENABLE
//! This is the interrupt to use when handling signal handshaking during
//! an on-going transaction.
void onTransactionHandshakeInterrupt(void *context, uint32_t /* interrupt */) {
  UartLinkManager *manager = static_cast<UartLinkManager *>(context);
  manager->getWakeInGpi()->SetTriggerFunction(GPIAoC::GPI_DISABLE);
  manager->getWakeInGpi()->ClearInterrupt();

  BaseType_t xHigherPriorityTaskWoken = 0;
  xTaskNotifyFromISR(manager->getTaskHandle(),
                     CHPP_TRANSPORT_SIGNAL_LINK_HANDSHAKE_IRQ, eSetBits,
                     &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#endif  // WAKE_HANDSHAKE_ENABLE

}  // anonymous namespace

UartLinkManager::UartLinkManager(struct ChppTransportState *context, UART *uart,
                                 uint8_t wakeOutPinNumber,
                                 uint8_t wakeInGpiNumber)
    : mTransportContext(context),
      mUart(uart),
      mWakeOutGpio(wakeOutPinNumber, IP_LOCK_GPIO),
      mWakeInGpi(wakeInGpiNumber) {
  mWakeOutGpio.SetDirection(GPIO::DIRECTION::OUTPUT);
  mWakeOutGpio.Clear();
  mTaskHandle = xTaskGetCurrentTaskHandle();
}

void UartLinkManager::init() {
  mUart->RegisterRxCallback(onUartRxInterrupt, this);
  mUart->EnableRxInterrupt();

  mWakeInGpi.SetInterruptHandler(onTransactionRequestInterrupt, this);
  // Use level triggered interrupts to handle possible race conditions.
  mWakeInGpi.SetTriggerFunction(GPIAoC::GPI_LEVEL_ACTIVE_HIGH);
  InterruptController::Instance()->InterruptEnable(
      IRQ_GPI0 + mWakeInGpi.GetGpiNumber(), Processor::Instance()->CoreID(),
      true /* enable */);
}

void UartLinkManager::deinit() {
  clearTxPacket();
  mWakeOutGpio.Clear();

  mUart->DisableRxInterrupt();

  InterruptController::Instance()->InterruptEnable(
      IRQ_GPI0 + mWakeInGpi.GetGpiNumber(), Processor::Instance()->CoreID(),
      false /* enable */);
  mWakeInGpi.SetTriggerFunction(GPIAoC::GPI_DISABLE);
}

bool UartLinkManager::prepareTxPacket(uint8_t *buf, size_t len) {
  bool success = !hasTxPacket();
  if (!success) {
    CHPP_LOGE("Cannot prepare packet while one pending");
  } else {
    mCurrentBuffer = buf;
    mCurrentBufferLen = len;
    setTransactionPending();
  }
  return success;
}

bool UartLinkManager::waitForHandshakeIrq(uint64_t timeoutNs) {
#if WAKE_HANDSHAKE_ENABLE
  bool success = false;
  Timer *timer = Timer::Instance();
  const TickType_t timeoutTicks =
      static_cast<TickType_t>(timer->NsToTicks(timeoutNs));

  // Treat as if the IRQ occurred in the timeout case. We check if the timer
  // was successfully cancelled to determine if we timed out or not.
  auto timeoutCallback = [](void *context) -> bool {
    onTransactionHandshakeInterrupt(context, 0);
    return false;  // one-shot
  };

  void *timerHandle;
  int rc = timer->EventAddAtOffset(timeoutTicks, timeoutCallback, this,
                                   &timerHandle);
  if (rc != 0) {
    CHPP_LOGE("Failed to set handshake timeout timer");
  } else {
    uint32_t signal = 0;
    while ((signal & CHPP_TRANSPORT_SIGNAL_LINK_HANDSHAKE_IRQ) == 0) {
      xTaskNotifyWait(
          0 /* ulBitsToClearOnEntry */,
          CHPP_TRANSPORT_SIGNAL_LINK_HANDSHAKE_IRQ /* ulBitsToClearOnExit */,
          &signal, portMAX_DELAY /* xTicksToWait */);
    }

    // If we cleared the notification while another one is in progress,
    // re-notify the task so it doesn't get lost.
    if ((signal & ~(CHPP_TRANSPORT_SIGNAL_LINK_HANDSHAKE_IRQ)) != 0) {
      xTaskNotify(mTaskHandle, 0, eNoAction);
    }

    // EventRemove() will not return 0 if the timer already fired.
    success = (timer->EventRemove(timerHandle) == 0);
  }

  return success;
#else
  return true;
#endif  // WAKE_HANDSHAKE_ENABLE
}

bool UartLinkManager::startTransaction() {
  bool success = true;
  mWakeInGpi.SetTriggerFunction(GPIAoC::GPI_DISABLE);

  // Check if a transaction is pending (either from an IRQ or from a pending
  // TX packet) before starting one. This check is required to avoid attempting
  // to start a transaction when it has already been handled (e.g. an IRQ has
  // signalled the transport layer, but it already had a packet to send and
  // handled them together).
  if (mTransactionPending.load()) {
    mWakeOutGpio.Set(true /* set */);

#if WAKE_HANDSHAKE_ENABLE
    mWakeInGpi.SetInterruptHandler(onTransactionHandshakeInterrupt, this);
    mWakeInGpi.SetTriggerFunction(GPIAoC::GPI_LEVEL_ACTIVE_HIGH);
#endif  // WAKE_HANDSHAKE_ENABLE

    if (waitForHandshakeIrq(kStartTimeoutNs)) {
      if (hasTxPacket()) {
        int bytesTransmitted = mUart->Tx(mCurrentBuffer, mCurrentBufferLen);

        // Deassert wake_out as soon as data is transmitted, per specifications.
        mWakeOutGpio.Clear();

        if (static_cast<size_t>(bytesTransmitted) != mCurrentBufferLen) {
          CHPP_LOGE("Failed to transmit data");
          success = false;
        }

      } else {
        // TODO: Wait for pulse width requirement per specifications.
        mWakeOutGpio.Clear();
      }

#if WAKE_HANDSHAKE_ENABLE
      mWakeInGpi.SetTriggerFunction(GPIAoC::GPI_LEVEL_ACTIVE_LOW);
      if (!waitForHandshakeIrq(kEndTimeoutNs)) {
        CHPP_LOGE("Wake handshaking end timed out");
        success = false;
      }
#endif  // WAKE_HANDSHAKE_ENABLE
    } else {
      CHPP_LOGE("Wake handshaking start timed out");
      success = false;
      mWakeOutGpio.Clear();
    }

    mTransactionPending = false;
  }

  // Re-enable the interrupt to handle transaction requests.
  mWakeInGpi.SetTriggerFunction(GPIAoC::GPI_DISABLE);
  mWakeInGpi.SetInterruptHandler(onTransactionRequestInterrupt, this);
  mWakeInGpi.SetTriggerFunction(GPIAoC::GPI_LEVEL_ACTIVE_HIGH);

  // Remove the TX packet to allow subsequent transmission.
  clearTxPacket();

  return success;
}

void UartLinkManager::pullRxSamples() {
  int ch;
  while ((ch = mUart->GetChar()) != EOF && mRxBufIndex < kRxBufSize) {
    mRxBuf[mRxBufIndex++] = static_cast<uint8_t>(ch);
  }
}

void UartLinkManager::processRxSamples() {
  mUart->DisableRxInterrupt();
  pullRxSamples();

  chppRxDataCb(mTransportContext, const_cast<uint8_t *>(mRxBuf), mRxBufIndex);
  mRxBufIndex = 0;
  mUart->EnableRxInterrupt();
}

bool UartLinkManager::hasTxPacket() const {
  return mCurrentBuffer != nullptr && mUart != nullptr;
}

void UartLinkManager::clearTxPacket() {
  mCurrentBuffer = nullptr;
}

}  // namespace chpp
