/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef CHRE_PLATFORM_FREERTOS_CONDITION_VARIABLE_IMPL_H_
#define CHRE_PLATFORM_FREERTOS_CONDITION_VARIABLE_IMPL_H_

#include "chre/platform/condition_variable.h"

namespace chre {

inline ConditionVariable::ConditionVariable() {
  mCvSemaphoreHandle = xSemaphoreCreateBinaryStatic(&mSemaphoreBuffer);
  if (mCvSemaphoreHandle == NULL) {
    FATAL_ERROR("failed to create cv semaphore");
  }
}

inline ConditionVariable::~ConditionVariable() {
  if (mCvSemaphoreHandle != NULL) {
    vSemaphoreDelete(mCvSemaphoreHandle);
  }
}

inline void ConditionVariable::notify_one() {
  xSemaphoreGive(mCvSemaphoreHandle);
}

inline void ConditionVariable::wait(Mutex &mutex) {
  mutex.unlock();
  xSemaphoreTake(mCvSemaphoreHandle, portMAX_DELAY /* xBlockTime */);
  mutex.lock();
}

inline bool ConditionVariable::wait_for(Mutex &mutex, Nanoseconds timeout) {
  if (!mTimerInitialized) {
    if (!mTimeoutTimer.init()) {
      FATAL_ERROR("Failed to initialize condition variable timer");
    } else {
      mTimerInitialized = true;
    }
  }

  mTimedOut = false;
  auto callback = [](void *data) {
    auto cbData = static_cast<ConditionVariable *>(data);
    cbData->mTimedOut = true;
    cbData->notify_one();
  };
  if (!mTimeoutTimer.set(callback, this, timeout)) {
    LOGE("Failed to set condition variable timer");
  }

  wait(mutex);
  if (mTimeoutTimer.isActive()) {
    if (!mTimeoutTimer.cancel()) {
      LOGD("Failed to cancel condition variable timer");
    }
  }
  return !mTimedOut;
}

inline void ConditionVariableBase::notify_one_from_isr() {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(mCvSemaphoreHandle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

}  // namespace chre

#endif  // CHRE_PLATFORM_FREERTOS_CONDITION_VARIABLE_IMPL_H_
