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

#include "chpp/platform/chpp_task_util.h"

#include "chpp/log.h"
#include "chre/platform/fatal_error.h"
#include "efw/include/timer.h"

namespace chpp {

TaskUtil::TaskUtil() {
  mCvSemaphoreHandle = xSemaphoreCreateBinaryStatic(&mSemaphoreBuffer);
  if (mCvSemaphoreHandle == nullptr) {
    FATAL_ERROR("failed to create cv semaphore");
  }
}

TaskUtil::~TaskUtil() {
  if (mCvSemaphoreHandle != nullptr) {
    vSemaphoreDelete(mCvSemaphoreHandle);
  }
}

void TaskUtil::suspend(uint64_t suspendTimeNs) {
  Timer *timer = Timer::Instance();
  const TickType_t timeoutTicks =
      static_cast<TickType_t>(timer->NsToTicks(suspendTimeNs));

  auto timeoutCallback = [](void *context) -> bool {
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(context);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return false;  // one-shot
  };

  void *timerHandle;
  int rc = timer->EventAddAtOffset(timeoutTicks, timeoutCallback,
                                   mCvSemaphoreHandle, &timerHandle);
  if (rc != 0) {
    CHPP_LOGE("Failed to set suspend timeout timer");
  } else {
    xSemaphoreTake(mCvSemaphoreHandle, portMAX_DELAY /* xBlockTime */);
  }
}

}  // namespace chpp
