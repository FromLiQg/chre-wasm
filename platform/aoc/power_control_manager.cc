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

#include "chre/platform/power_control_manager.h"

#include "chre/core/event_loop_manager.h"

namespace chre {

PowerControlManagerBase::PowerControlManagerBase() : mHostIsAwake(true) {}

void PowerControlManagerBase::onHostWakeSuspendEvent(bool awake) {
  if (mHostIsAwake != awake) {
    mHostIsAwake = awake;

    if (!awake) {
      EventLoopManagerSingleton::get()
          ->getHostCommsManager()
          .resetBlameForNanoappHostWakeup();
    }

    EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
        awake ? CHRE_EVENT_HOST_AWAKE : CHRE_EVENT_HOST_ASLEEP,
        nullptr /* eventData */, nullptr /* freeCallback */);
  }
}

void PowerControlManager::postEventLoopProcess(size_t /* numPendingEvents */) {
  // TODO: stubbed out, Implement this.
}

bool PowerControlManager::hostIsAwake() {
  return mHostIsAwake;
}

}  // namespace chre
