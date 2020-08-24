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

#ifndef CHRE_PLATFORM_AOC_PLATFORM_AUDIO_BASE_H_
#define CHRE_PLATFORM_AOC_PLATFORM_AUDIO_BASE_H_

#include <cstring>

#include "chre/platform/aoc/audio_controller.h"
#include "chre/platform/platform_audio.h"

namespace chre {

/**
 * The base PlatformAudio class for AoC to inject platform
 * specific functionality from. Instantiates an Audio Controller object,
 * which handles communications, data buffering, and audio data event
 * dispatch for CHRE.
 */
class PlatformAudioBase {
 protected:
  AudioController mAudioController;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_AOC_PLATFORM_AUDIO_BASE_H_
