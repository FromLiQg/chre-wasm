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

// TODO: Use a more primitive construct, e.g. xTaskNotify/xTaskNotifyWait
void chppPlatformNotifierInit(struct ChppNotifier *notifier) {
  notifier->signal = 0;
  notifier->cvSemaphore.handle =
      xSemaphoreCreateBinaryStatic(&notifier->cvSemaphore.staticSemaphore);
  if (notifier->cvSemaphore.handle == nullptr) {
    // TODO: Use CHPP_ASSERT
    CHPP_LOGE("Failed to initialize CHPP notifier");
  } else {
    chppMutexInit(&notifier->mutex);
  }
}

void chppPlatformNotifierDeinit(struct ChppNotifier *notifier) {
  if (notifier->cvSemaphore.handle != nullptr) {
    chppMutexDeinit(&notifier->mutex);
    vSemaphoreDelete(notifier->cvSemaphore.handle);
  }
}

uint32_t chppPlatformNotifierWait(struct ChppNotifier *notifier) {
  if (notifier->cvSemaphore.handle != nullptr) {
    chppMutexLock(&notifier->mutex);

    while (notifier->signal == 0) {
      chppMutexUnlock(&notifier->mutex);
      const TickType_t timeout = portMAX_DELAY;  // block indefinitely
      xSemaphoreTake(notifier->cvSemaphore.handle, timeout);
      chppMutexLock(&notifier->mutex);
    }
    uint32_t signal = notifier->signal;
    notifier->signal = 0;

    chppMutexUnlock(&notifier->mutex);
    return signal;
  } else {
    return 0;
  }
}

void chppPlatformNotifierSignal(struct ChppNotifier *notifier,
                                uint32_t signal) {
  if (notifier->cvSemaphore.handle != nullptr) {
    chppMutexLock(&notifier->mutex);

    notifier->signal |= signal;
    xSemaphoreGive(notifier->cvSemaphore.handle);

    chppMutexUnlock(&notifier->mutex);
  }
}
