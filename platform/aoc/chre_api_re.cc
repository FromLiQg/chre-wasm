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
#include <cinttypes>

#include "chre/platform/log.h"
#include "chre/util/macros.h"
#include "chre_api/chre/re.h"

// The log buffer size is limited by the frame size defined in :
// {AOC_TOP}/build/{THIS_PLATFORM}/toolchain.mk
// There is no limit for the linux simulator, 512 bytes for QEMU
// and 280 bytes for a32, so we pick the maximum common size (i..e. the
// minimum of the allowable frame sizes), with a small allowance for the
// size of the function itself
static constexpr size_t kChreLogBufferSize = 240;

void chreLog(enum chreLogLevel level, const char *formatStr, ...) {
  char logBuf[kChreLogBufferSize];
  va_list args;

  va_start(args, formatStr);
  vsnprintf(logBuf, sizeof(logBuf), formatStr, args);
  va_end(args);

  switch (level) {
    case CHRE_LOG_ERROR:
      LOGE("%s", logBuf);
      break;
    case CHRE_LOG_WARN:
      LOGW("%s", logBuf);
      break;
    case CHRE_LOG_INFO:
      LOGI("%s", logBuf);
      break;
    case CHRE_LOG_DEBUG:
    default:
      LOGD("%s", logBuf);
  }
}
