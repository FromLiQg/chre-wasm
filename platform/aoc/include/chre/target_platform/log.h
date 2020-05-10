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

#ifndef __FILENAME__
#ifdef __BASE_FILE__
#define __FILENAME__ __BASE_FILE__
#else
#define __FILENAME__ __FILE__
#endif  // __BASE_FILE__
#endif  // __FILE_NAME__

#include <stdio.h>

// TODO: b/146164384 - We will need to batch logs rather than send them
// one at a time to avoid waking the AP.
#define CHRE_AOC_LOG(level, fmt, ...)                               \
  printf("CHRE:%s %s:%d\t" fmt "\n", level, __FILENAME__, __LINE__, \
         ##__VA_ARGS__)
#define LOGE(fmt, ...) CHRE_AOC_LOG("E", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) CHRE_AOC_LOG("W", fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) CHRE_AOC_LOG("I", fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) CHRE_AOC_LOG("D", fmt, ##__VA_ARGS__)

#endif  // CHRE_PLATFORM_AOC_LOG_H_
