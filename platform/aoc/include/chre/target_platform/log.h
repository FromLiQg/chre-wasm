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
#ifndef CHRE_PLATFORM_AOC_LOG_H_
#define CHRE_PLATFORM_AOC_LOG_H_

#include <chre.h>
#include <stdio.h>
#include "efw_log.h"

#ifndef __FILENAME__
#ifdef __BASE_FILE__
#define __FILENAME__ __BASE_FILE__
#else
#define __FILENAME__ __FILE__
#endif  // __BASE_FILE__
#endif  // __FILE_NAME__

#ifndef CHRE_DL_VERBOSE
#define CHRE_DL_VERBOSE false
#endif  // CHRE_DL_VERBOSE

// TODO(b/149317051): The printf in the below macro is needed until CHRE can log
// to the AP before the daemon has connected to AoC.
#define CHRE_AOC_LOG(level, fmt, ...)                               \
  do {                                                              \
    CHRE_LOG_PREAMBLE                                               \
    chre::log(level, fmt, ##__VA_ARGS__);                           \
    DBG_MSG_RAW(255, "CHRE:%s:%d\t" fmt "", __FILENAME__, __LINE__, \
                ##__VA_ARGS__);                                     \
    CHRE_LOG_EPILOGUE                                               \
  } while (0)

#define LOGE(fmt, ...) CHRE_AOC_LOG(CHRE_LOG_ERROR, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) CHRE_AOC_LOG(CHRE_LOG_WARN, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) CHRE_AOC_LOG(CHRE_LOG_INFO, fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) CHRE_AOC_LOG(CHRE_LOG_DEBUG, fmt, ##__VA_ARGS__)

#if CHRE_DL_VERBOSE
#define LOGV(fmt, ...) LOGD(fmt, ##__VA_ARGS__)
#else
#define LOGV(fmt, ...)
#endif  // CHRE_DL_VERBOSE

namespace chre {

void log(enum chreLogLevel level, const char *formatStr, ...);
void vaLog(enum chreLogLevel level, const char *format, va_list args);

}  // namespace chre

#endif  // CHRE_PLATFORM_AOC_LOG_H_
