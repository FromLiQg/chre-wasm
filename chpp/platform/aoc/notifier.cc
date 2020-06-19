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

void chppPlatformNotifierInit(struct ChppNotifier *notifier) {
  notifier->signaled = false;
  notifier->shouldExit = false;
  notifier->cvSemaphore.handle =
      xSemaphoreCreateBinaryStatic(&notifier->cvSemaphore.staticSemaphore);
  if (notifier->cvSemaphore.handle == nullptr) {
    // TODO: Use CHPP_ASSERT
    LOGE("Failed to initialize CHPP notifier");
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

bool chppPlatformNotifierWait(struct ChppNotifier *notifier) {
  if (notifier->cvSemaphore.handle != nullptr) {
    chppMutexLock(&notifier->mutex);

    while (!notifier->signaled && !notifier->shouldExit) {
      chppMutexUnlock(&notifier->mutex);
      const TickType_t timeout = portMAX_DELAY;  // block indefinitely
      xSemaphoreTake(notifier->cvSemaphore.handle, timeout);
      chppMutexLock(&notifier->mutex);
    }
    notifier->signaled = false;

    bool shouldExit = notifier->shouldExit;
    chppMutexUnlock(&notifier->mutex);
    return !shouldExit;
  } else {
    return false;
  }
}

void chppPlatformNotifierEvent(struct ChppNotifier *notifier) {
  if (notifier->cvSemaphore.handle != nullptr) {
    chppMutexLock(&notifier->mutex);

    notifier->signaled = true;
    xSemaphoreGive(notifier->cvSemaphore.handle);

    chppMutexUnlock(&notifier->mutex);
  }
}

void chppPlatformNotifierExit(struct ChppNotifier *notifier) {
  if (notifier->cvSemaphore.handle != nullptr) {
    chppMutexLock(&notifier->mutex);

    notifier->shouldExit = true;
    xSemaphoreGive(notifier->cvSemaphore.handle);

    chppMutexUnlock(&notifier->mutex);
  }
}
