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

#ifndef CHRE_PLATFORM_SLPI_PLATFORM_LOG_BASE_H_
#define CHRE_PLATFORM_SLPI_PLATFORM_LOG_BASE_H_

#include "chre/platform/mutex.h"
#include "chre/platform/shared/log_buffer.h"
#include "chre_api/chre/re.h"

namespace chre {

class PlatformLogBase : public LogBufferCallbackInterface {
 public:
  PlatformLogBase();

  void onLogsReady(LogBuffer *logBuffer) final;

  void onLogsSentToHost();

  LogBuffer *getLogBuffer() {
    return &mLogBuffer;
  }

  uint8_t *getTempLogBufferData() {
    return mTempLogBufferData;
  }

 protected:
  /*
   * @return The LogBuffer log level for the given CHRE log level.
   */
  LogBufferLogLevel chreToLogBufferLogLevel(chreLogLevel chreLogLevel);

  LogBuffer mLogBuffer;
  uint8_t mTempLogBufferData[CHRE_MESSAGE_TO_HOST_MAX_SIZE];
  uint8_t mLogBufferData[CHRE_MESSAGE_TO_HOST_MAX_SIZE];

  bool mLogFlushToHostPending = false;
  bool mLogsBecameReadyWhileFlushPending = false;
  Mutex mFlushLogsMutex;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_SLPI_PLATFORM_LOG_BASE_H_
