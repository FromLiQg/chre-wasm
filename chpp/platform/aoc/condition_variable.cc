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

#include "chpp/platform/platform_condition_variable.h"

#include "chpp/log.h"
#include "chre/platform/fatal_error.h"
#include "efw/include/interrupt_controller.h"
#include "efw/include/timer.h"

namespace {

bool wait(struct ChppConditionVariable *cv, struct ChppMutex *mutex) {
  chppMutexUnlock(mutex);
  BaseType_t rc = xSemaphoreTake(cv->semaphoreHandle, portMAX_DELAY);
  chppMutexLock(mutex);

  return (rc == pdTRUE);
}

bool timeoutCallback(void *context) {
  auto *cv = static_cast<struct ChppConditionVariable *>(context);
  chppPlatformConditionVariableSignal(cv);
  return false;  // one-shot
}

}  // anonymous namespace

void chppPlatformConditionVariableInit(struct ChppConditionVariable *cv) {
  cv->semaphoreHandle = xSemaphoreCreateBinaryStatic(&cv->staticSemaphore);
  if (cv->semaphoreHandle == NULL) {
    FATAL_ERROR("Failed to create cv semaphore");
  }
}

void chppPlatformConditionVariableDeinit(struct ChppConditionVariable *cv) {
  if (cv->semaphoreHandle != NULL) {
    vSemaphoreDelete(cv->semaphoreHandle);
  }
}

bool chppPlatformConditionVariableWait(struct ChppConditionVariable *cv,
                                       struct ChppMutex *mutex) {
  return wait(cv, mutex);
}

bool chppPlatformConditionVariableTimedWait(struct ChppConditionVariable *cv,
                                            struct ChppMutex *mutex,
                                            uint64_t timeoutNs) {
  bool success = false;
  Timer *timer = Timer::Instance();
  const TickType_t timeoutTicks =
      static_cast<TickType_t>(timer->NsToTicks(timeoutNs));

  void *timerHandle;
  int rc =
      timer->EventAddAtOffset(timeoutTicks, timeoutCallback, cv, &timerHandle);
  if (rc != 0) {
    CHPP_LOGE("Failed to set cv timeout timer");
  } else {
    success = wait(cv, mutex) && (timer->EventRemove(timerHandle) == 0);
  }

  return success;
}

void chppPlatformConditionVariableSignal(struct ChppConditionVariable *cv) {
  if (InterruptController::Instance()->IsInterruptContext()) {
    BaseType_t xHigherPriorityTaskWoken = 0;
    xSemaphoreGiveFromISR(cv->semaphoreHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  } else {
    xSemaphoreGive(cv->semaphoreHandle);
  }
}
