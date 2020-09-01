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

#ifndef CHPP_LOG_H_
#define CHPP_LOG_H_

#include <chre.h>
#include <inttypes.h>

#include "FreeRTOS.h"
#include "task.h"

#define CHPP_AOC_LOG(level, fmt, ...)                                    \
  chreLog(level, "%s: " fmt, pcTaskGetName(xTaskGetCurrentTaskHandle()), \
          ##__VA_ARGS__)

#define CHPP_LOGE(fmt, ...) CHPP_AOC_LOG(CHRE_LOG_ERROR, fmt, ##__VA_ARGS__)
#define CHPP_LOGW(fmt, ...) CHPP_AOC_LOG(CHRE_LOG_WARN, fmt, ##__VA_ARGS__)
#define CHPP_LOGI(fmt, ...) CHPP_AOC_LOG(CHRE_LOG_INFO, fmt, ##__VA_ARGS__)
#define CHPP_LOGD(fmt, ...) CHPP_AOC_LOG(CHRE_LOG_DEBUG, fmt, ##__VA_ARGS__)

#define CHPP_LOG_OOM(fmt, ...) \
  CHPP_AOC_LOG(CHRE_LOG_ERROR, "(OOM) " fmt, ##__VA_ARGS__)

#endif  // CHPP_LOG_H_
