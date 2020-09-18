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

#include "chre/platform/system_timer.h"
#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre/util/time.h"

#include "efw/include/timer.h"

namespace {

// The timer dispatch thread is notifed from a
// timer interrupt context, and there are context checks in the FreeRTOS code
// (i.e. the xxGive and xxGiveFromISR are not interchangeable). Since there
// are no interrupts in a simulated platform, we end up with two different
// notification mechanisms that accomplish the same purpose.
void wakeupDispatchThreadFromIsr(TaskHandle_t &handle) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

}  // namespace

namespace chre {

bool SystemTimerBase::systemTimerNotifyCallback(void *context) {
  SystemTimer *pTimer = static_cast<SystemTimer *>(context);
  if (pTimer != nullptr) {
    wakeupDispatchThreadFromIsr(pTimer->mTimerCbDispatchThreadHandle);
  }

  // The EFW timer callback is setup in such a way that returning 'true'
  // here reschedules the timer, while returning 'false' does not.
  // Since we're interested in a one-shot timer, we return 'false' here.
  return false;
}

void SystemTimerBase::timerCallbackDispatch(void *context) {
  SystemTimer *pTimer = static_cast<SystemTimer *>(context);
  if (pTimer == nullptr) {
    FATAL_ERROR("Null System Timer");
  }

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Obtain pointer to callback before checking its value to avoid race
    // with cancel().
    SystemTimerCallback *callback = pTimer->mCallback;
    if (callback != nullptr) {
      callback(pTimer->mData);
    }
  }
}

SystemTimer::SystemTimer() {}

SystemTimer::~SystemTimer() {
  // cancel an existing timer if any
  cancel();
  // Delete the timer dispatch thread if it was created
  if (mTimerCbDispatchThreadHandle != nullptr) {
    vTaskDelete(mTimerCbDispatchThreadHandle);
    mTimerCbDispatchThreadHandle = nullptr;
  }
}

bool SystemTimer::init() {
  // TODO(b/168526254): Balance this priority with other CHRE tasks.
  constexpr UBaseType_t kTaskPriority = tskIDLE_PRIORITY + 15;
  const char *const kTaskName = "ChreTimerCB";

  mTimerCbDispatchThreadHandle =
      xTaskCreateStatic(timerCallbackDispatch, kTaskName, kStackDepthWords,
                        this, kTaskPriority, mTaskStack, &mTaskBuffer);
  if (mTimerCbDispatchThreadHandle != nullptr) {
    mInitialized = true;
  } else {
    LOGE("Failed to create Timer Dispatch Thread");
  }

  return mInitialized;
}

bool SystemTimer::set(SystemTimerCallback *callback, void *data,
                      Nanoseconds delay) {
  bool success = false;

  if (mInitialized) {
    // Cancel the existing timer to avoid a race where the old timer expires
    // when setting up the new timer.
    cancel();

    mCallback = callback;
    mData = data;

    Timer *timer = Timer::Instance();
    int rc =
        timer->EventAddAtOffset(timer->NsToTicks(delay.toRawNanoseconds()),
                                systemTimerNotifyCallback, this, &mTimerHandle);

    if (rc != 0) {
      LOGE("Failed to set timer");
    } else {
      success = true;
    }
  }

  return success;
}

bool SystemTimer::cancel() {
  int rc = -1;
  if (mTimerHandle != nullptr) {
    rc = Timer::Instance()->EventRemove(mTimerHandle);
    mTimerHandle = nullptr;
    mCallback = nullptr;
  }

  return (rc == 0) ? true : false;
}

bool SystemTimer::isActive() {
  // TODO: stubbed out, Implement this.
  return false;
}

}  // namespace chre
