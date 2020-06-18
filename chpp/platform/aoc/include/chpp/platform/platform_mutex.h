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

#ifndef CHPP_PLATFORM_MUTEX_H_
#define CHPP_PLATFORM_MUTEX_H_

#include "chpp/platform/log.h"
#include "FreeRTOS.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encapsulates a mutex handle and its associated semaphore structure.
 * The static semaphore is used to avoid heap allocations.
 */
struct ChppMutex {
  SemaphoreHandle_t semaphoreHandle;
  StaticSemaphore_t staticSemaphore;
};

static inline void chppMutexInit(struct ChppMutex *mutex) {
  mutex->semaphoreHandle = xSemaphoreCreateMutexStatic(&mutex->staticSemaphore);
}

static inline void chppMutexDeinit(struct ChppMutex *mutex) {
  if (mutex->semaphoreHandle) {
    vSemaphoreDelete(mutex->semaphoreHandle);
  }
}

static inline void chppMutexLock(struct ChppMutex *mutex) {
  TickType_t blockForever = portMAX_DELAY;
  if (pdTRUE != xSemaphoreTake(mutex->semaphoreHandle, blockForever)) {
    // TODO: Use CHPP_ASSERT
    LOGE("Failed to lock mutex");
  }
}

static inline void chppMutexUnlock(struct ChppMutex *mutex) {
  if (pdTRUE != xSemaphoreGive(mutex->semaphoreHandle)) {
    LOGE("Failed to properly unlock mutex!");
  }
}

#ifdef __cplusplus
}
#endif

#endif  // CHPP_PLATFORM_MUTEX_H_
