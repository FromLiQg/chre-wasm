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
#include <cinttypes>
#include "chre/core/event_loop_manager.h"
#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre/platform/system_time.h"

namespace chre {

PlatformAudio::PlatformAudio() {}

PlatformAudio::~PlatformAudio() {
  mAudioController.Stop();
}

void PlatformAudio::init() {
  mAudioController.Start();
  mAudioController.SetUpRemoteCore();
}

size_t PlatformAudio::getSourceCount() {
  return AudioController::kMaxAocMicrophones;
}

bool PlatformAudio::getAudioSource(uint32_t handle,
                                   struct chreAudioSource *source) const {
  bool success = false;
  if ((handle < AudioController::kMaxAocMicrophones) && (source != nullptr)) {
    memcpy(source, mAudioController.GetSource(handle), sizeof(chreAudioSource));
    success = true;
  }

  return success;
}

void PlatformAudio::setHandleEnabled(uint32_t handle, bool enabled) {
  mAudioController.SetEnabled(handle, enabled);
}

void PlatformAudio::cancelAudioDataEventRequest(uint32_t handle) {
  mAudioController.ReleaseAudio(handle);
}

bool PlatformAudio::requestAudioDataEvent(uint32_t handle, uint32_t numSamples,
                                          Nanoseconds eventDelay) {
  // TODO: Event delay
  bool success = mAudioController.RequestAudio(handle);
  LOGV("Request audio data for hdl %u, numSamples %u @ %" PRIu64
       ", success: %d",
       handle, numSamples, SystemTime::getMonotonicTime().toRawNanoseconds(),
       success);

  return success;
}

void PlatformAudio::releaseAudioDataEvent(
    struct chreAudioDataEvent * /*event*/) {
  mAudioController.OnBufferReleased();
}

}  // namespace chre
