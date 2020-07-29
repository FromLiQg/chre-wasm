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

#include "chre/platform/fatal_error.h"
#include "efw/include/interrupt_controller.h"

void chppPlatformConditionVariableInit(
    struct ChppConditionVariable *condition_variable) {
  condition_variable->semaphoreHandle =
      xSemaphoreCreateBinaryStatic(&condition_variable->staticSemaphore);
  if (condition_variable->semaphoreHandle == NULL) {
    FATAL_ERROR("Failed to create cv semaphore");
  }
}

void chppPlatformConditionVariableDeinit(
    struct ChppConditionVariable *condition_variable) {
  if (condition_variable->semaphoreHandle != NULL) {
    vSemaphoreDelete(condition_variable->semaphoreHandle);
  }
}

bool chppPlatformConditionVariableWait(
    struct ChppConditionVariable *condition_variable, struct ChppMutex *mutex) {
  chppMutexUnlock(mutex);
  BaseType_t rc = xSemaphoreTake(condition_variable->semaphoreHandle,
                                 portMAX_DELAY);  // block indefinitely
  chppMutexLock(mutex);

  return (rc == pdTRUE);
}

void chppPlatformConditionVariableSignal(
    struct ChppConditionVariable *condition_variable) {
  if (InterruptController::Instance()->IsInterruptContext()) {
    BaseType_t xHigherPriorityTaskWoken = 0;
    xSemaphoreGiveFromISR(condition_variable->semaphoreHandle,
                          &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  } else {
    xSemaphoreGive(condition_variable->semaphoreHandle);
  }
}
