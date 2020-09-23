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

#ifndef CHPP_PLATFORM_TASK_UTIL_H_
#define CHPP_PLATFORM_TASK_UTIL_H_

#include "FreeRTOS.h"
#include "semphr.h"

namespace chpp {

/**
 * A utility class for task related operations.
 */
class TaskUtil {
 public:
  TaskUtil();
  ~TaskUtil();

  /**
   * Suspends the task for a specified period of time.
   *
   * @param suspendTimeNs The time in nanoseconds to suspend.
   */
  void suspend(uint64_t suspendTimeNs);

 private:
  //! The semaphore handle
  SemaphoreHandle_t mCvSemaphoreHandle;
  //! Buffer used to store state used by the semaphore.
  StaticSemaphore_t mSemaphoreBuffer;
};

}  // namespace chpp

#endif  // CHPP_PLATFORM_TASK_UTIL_H_
