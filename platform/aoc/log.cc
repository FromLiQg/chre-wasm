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

#include "chre/target_platform/log.h"

#include "chre/core/event_loop_manager.h"
#include "chre/platform/system_time.h"

#include <endian.h>

namespace chre {
namespace {
constexpr size_t kChreLogBufferSize = CHRE_MESSAGE_TO_HOST_MAX_SIZE;
char logBuffer[kChreLogBufferSize];
}  // namespace

void log(enum chreLogLevel level, const char *formatStr, ...) {
  va_list args;
  va_start(args, formatStr);
  vaLog(level, formatStr, args);
  va_end(args);
}

// TODO: b/146164384 - We will need to batch logs rather than send them
// one at a time to avoid waking the AP.
void vaLog(enum chreLogLevel level, const char *format, va_list args) {
  auto &hostCommsMgr =
      chre::EventLoopManagerSingleton::get()->getHostCommsManager();

  // See host_messages.fbs for the log message format.
  size_t logBufIndex = 0;
  logBuffer[logBufIndex] = level + 1;
  logBufIndex++;

  uint64_t currentTimeNs = SystemTime::getMonotonicTime().toRawNanoseconds();
  memcpy(&logBuffer[logBufIndex], &currentTimeNs, sizeof(uint64_t));
  logBufIndex += sizeof(uint64_t);

  int msgLen = vsnprintf(&logBuffer[logBufIndex],
                         kChreLogBufferSize - logBufIndex, format, args);
  if (msgLen >= 0) {
    // msgLen doesn't include the terminating null char.
    logBufIndex += msgLen + 1;

    hostCommsMgr.sendLogMessage(reinterpret_cast<uint8_t *>(logBuffer),
                                logBufIndex);
  }
}

}  // namespace chre
