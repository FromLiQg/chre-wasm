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

#include "chre/platform/platform_audio.h"
#include "chre/platform/log.h"

namespace chre {

PlatformAudio::PlatformAudio() {}

PlatformAudio::~PlatformAudio() {}

void PlatformAudio::init() {
  LOGE("Stubbed out, to be implemented");
}

size_t PlatformAudio::getSourceCount() {
  LOGE("Stubbed out, to be implemented");
  return 0;
}

bool PlatformAudio::getAudioSource(uint32_t /*handle*/,
                                   struct chreAudioSource * /*source*/) const {
  LOGE("Stubbed out, to be implemented");
  return false;
}

void PlatformAudio::setHandleEnabled(uint32_t /*handle*/, bool /*enabled*/) {
  LOGE("Stubbed out, to be implemented");
}

void PlatformAudio::cancelAudioDataEventRequest(uint32_t /*handle*/) {
  LOGE("Stubbed out, to be implemented");
}

bool PlatformAudio::requestAudioDataEvent(uint32_t /*handle*/,
                                          uint32_t /*numSamples*/,
                                          Nanoseconds /*eventDelay*/) {
  LOGE("Stubbed out, to be implemented");
  return false;
}

}  // namespace chre