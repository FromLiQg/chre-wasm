/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "chre/platform/context.h"
#include "chre/core/event_loop_manager.h"

namespace {

chre::EventLoop *gEventLoop = nullptr;

}  // namespace

namespace chre {

EventLoop *getCurrentEventLoop() {
  if (gEventLoop == nullptr) {
    gEventLoop = &chre::EventLoopManagerSingleton::get()->getEventLoop();
  }

  // note: on a multi-threaded implementation, we would likely use thread-local
  // storage here if available, or a map from thread ID --> eventLoop
  return gEventLoop;
}

}  // namespace chre
