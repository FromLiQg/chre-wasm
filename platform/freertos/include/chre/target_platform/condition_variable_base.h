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

#ifndef CHRE_PLATFORM_FREERTOS_CONDITION_VARIABLE_BASE_H_
#define CHRE_PLATFORM_FREERTOS_CONDITION_VARIABLE_BASE_H_

#include "chre/platform/mutex.h"
#include "chre/platform/system_timer.h"
#include "chre/target_platform/fatal_error.h"

#include "FreeRTOS.h"
#include "semphr.h"

namespace chre {

/**
 * @brief FreeRTOS implementation of the condition variable
 *        using FreeRTOS primitives
 *
 * The following base class implements the CHRE ConditionVariable
 * interface for FreeRTOS. Some points to be noted:
 * - We have the benefit of not needing to support notify_all(),
 *   as we only provide the notify_one() abstraction to common code.
 * - We don't have the requirement where more than 1 thread can wait
 *   on a given condition variable in parallel  (which kind of goes against the
 *   CV name, but really we were looking for something that let us implement
 *   @ref FixedSizeBlockingQueue while keeping familiarity to the C++11 thread
 *   support library
 * - Currently, there's exactly one user of the condition variable in the common
 *   code, and that's the FixedSizeBlockingQueue used by the EventLoop. Hence,
 *   we can codify assumptions that are well supported by the current code into
 *   the API. One example of this would be ensuring the caller can handle
 *   spurious wakeups (which is fine for CHRE) as the worst side effect of this
 *   is an extra iteration of the while-empty-queue loop.
 */
class ConditionVariableBase {
 protected:
  // Since, per CHRE specification, only one thread is ever
  // going to be blocked on this condition variable, all we
  // need is a Binary Semaphore. Note that:
  // - while std::condition_variable allows for multiple threads
  //   to wait on the same condition variable object
  //   concurrently, the CHRE platform implementation is only required
  //   to allow for a single waiting thread.
  // - The calling code has to allow for spurious wakeups

  SemaphoreHandle_t mCvSemaphoreHandle;

  /**
   * Waits on a semaphore for a specified amount of timer ticks,
   * locking the mutex before returning. If the timer ticks argument is
   * 0, blocks indefinitely.
   *
   * @return true if the semaphore was successfully taken
   */
  bool waitWithTimeout(Mutex &mutex, const TickType_t &timeoutTicks) {
    mutex.unlock();
    BaseType_t rc = xSemaphoreTake(mCvSemaphoreHandle, timeoutTicks);
    mutex.lock();

    return (rc == pdTRUE);
  }

  /**
   * Initialize a Static FreeRTOS semaphore. This function is called
   * from the derived class constructor
   */
  void initStaticSemaphore() {
    mCvSemaphoreHandle = xSemaphoreCreateBinaryStatic(&mCvStaticSemaphore);
    if (mCvSemaphoreHandle == NULL) {
      FATAL_ERROR("failed to create cv semaphore");
    }
  }

 private:
  StaticSemaphore_t mCvStaticSemaphore;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_FREERTOS_CONDITION_VARIABLE_BASE_H_
