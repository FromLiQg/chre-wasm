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

void chreLog(enum chreLogLevel level, const char *formatStr, ...) {
  va_list args;
  va_start(args, formatStr);

  // TODO(karthikmb): Remove this once log buffering is implemented.
  char logBuf[500];
  va_list vsnargs;
  va_copy(vsnargs, args);
  vsnprintf(logBuf, sizeof(logBuf), formatStr, args);
  va_end(vsnargs);
  printf("CHRE: %s\n", logBuf);

  chre::vaLog(level, formatStr, args);
  va_end(args);
}
