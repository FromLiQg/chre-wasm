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
  initStaticSemaphore();
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
  const TickType_t timeout = portMAX_DELAY;  // block indefinitely
  waitWithTimeout(mutex, timeout);
}

inline bool ConditionVariable::wait_for(Mutex &mutex, Nanoseconds timeout) {
  const TickType_t timeoutTicks = static_cast<TickType_t>(
      Timer::Instance()->NsToTicks(timeout.toRawNanoseconds()));
  return waitWithTimeout(mutex, timeoutTicks);
}

}  // namespace chre

#endif  // CHRE_PLATFORM_FREERTOS_CONDITION_VARIABLE_IMPL_H_
