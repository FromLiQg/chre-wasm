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

// We work with both simulated (linux port of FreeRTOS), and real (a32)
// target platforms. The timer dispatch thread is notifed from a
// timer interrupt context, and there are context checks in the FreeRTOS code
// (i.e. the xxGive and xxGiveFromISR are not interchangeable). Since there
// are no interrupts in a simulated platform, we end up with two different
// notification mechanisms that accomplish the same purpose.
void wakeupDispatchThread(TaskHandle_t &handle) {
#ifdef CHRE_PLATFORM_IS_SIMULATED
  // no ISRs in simulated platforms!
  xTaskNotifyGive(handle);
#else
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(handle, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif
}

}  // namespace

namespace chre {

TaskHandle_t SystemTimerBase::mTimerCbDispatchThreadHandle = NULL;

bool SystemTimerBase::systemTimerNotifyCallback(void *context) {
  SystemTimer *pTimer = static_cast<SystemTimer *>(context);
  if (pTimer != nullptr) {
    wakeupDispatchThread(mTimerCbDispatchThreadHandle);
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
    if ((pTimer != nullptr) && (pTimer->mCallback != nullptr)) {
      pTimer->mCallback(pTimer->mData);
    }
  }
}

SystemTimer::SystemTimer() {}

SystemTimer::~SystemTimer() {
  // cancel an existing timer if any
  cancel();
  // Delete the timer dispatch thread if it was created
  if (mTimerCbDispatchThreadHandle != NULL) {
    vTaskDelete(mTimerCbDispatchThreadHandle);
  }
}

bool SystemTimer::init() {
  BaseType_t rc =
      xTaskCreate(timerCallbackDispatch, "TimerCbDispatch", 1024, this,
                  tskIDLE_PRIORITY + 2, &mTimerCbDispatchThreadHandle);
  if (pdTRUE == rc) {
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
    // TODO: b/146374655 - investigate/handle possible race condition here
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
  }

  return (rc == 0) ? true : false;
}

bool SystemTimer::isActive() {
  // TODO: stubbed out, Implement this.
  return false;
}

}  // namespace chre
