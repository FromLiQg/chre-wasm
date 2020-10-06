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

#include "chre/platform/shared/log_buffer.h"

#include <cstdarg>
#include <cstdio>

namespace chre {

LogBuffer::LogBuffer(LogBufferCallbackInterface *callback, void *buffer,
                     size_t bufferSize) {
  // TODO(b/146164384): Implement
}

void LogBuffer::handleLog(LogBufferLogLevel logLevel, uint32_t timestampMs,
                          const char *log) {
  // TODO(b/146164384): Implement
}

size_t LogBuffer::copyLogs(void *destination, size_t size) {
  // TODO(b/146164384): Implement
  return 0;
}

void LogBuffer::transferTo(LogBuffer &buffer) {
  // TODO(b/146164384): Implement
}

void updateNotificationSetting(LogBufferNotificationSetting setting,
                               size_t thresholdBytes) {
  // TODO(b/146164384): Implement
}

}  // namespace chre
