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

#include <stdbool.h>

#include "chpp/macros.h"
#include "chpp/memory.h"
#include "chpp/notifier.h"
#include "efw/include/interrupt_controller.h"

void chppPlatformNotifierInit(struct ChppNotifier *notifier) {
  notifier->task = xTaskGetCurrentTaskHandle();
  xTaskNotifyStateClear(notifier->task);
}

void chppPlatformNotifierDeinit(struct ChppNotifier *notifier) {}

uint32_t chppPlatformNotifierWait(struct ChppNotifier *notifier) {
  uint32_t signal;
  xTaskNotifyWait(0 /* ulBitsToClearOnEntry */,
                  UINT32_MAX /* ulBitsToClearOnExit */, &signal,
                  portMAX_DELAY /* xTicksToWait */);
  return signal;
}

uint32_t chppPlatformNotifierTimedWait(struct ChppNotifier *notifier,
                                       uint64_t /* timeoutNs */) {
  // TODO: Implement this
  return chppPlatformNotifierWait(notifier);
}

void chppPlatformNotifierSignal(struct ChppNotifier *notifier,
                                uint32_t signal) {
  if (InterruptController::Instance()->IsInterruptContext()) {
    BaseType_t xHigherPriorityTaskWoken = 0;
    xTaskNotifyFromISR(notifier->task, signal, eSetBits,
                       &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  } else {
    xTaskNotify(notifier->task, signal, eSetBits);
  }
}
