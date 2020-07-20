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

#ifndef CHRE_PLATFORM_SHARED_DRAM_UTIL_H_
#define CHRE_PLATFORM_SHARED_DRAM_UTIL_H_

#include "chre/platform/shared/memory.h"
#include "chre/util/non_copyable.h"

namespace chre {

/**
 * This is an RAII-style class that enables and disables the active request
 * for DRAM following the scope of the guard. The concept is the same as
 * std::lock_guard.
 */
class DramGuard : public NonCopyable {
 public:
  /**
   * Constructs a DramGuard and enables access to DRAM.
   */
  DramGuard() {
    // TODO(stange): Change to FATAL_ERROR once this is guaranteed.
    if (!requestDramAccess(true)) {
      LOGE("Unable to request DRAM access");
    }
  }

  /**
   * Deconstructs a DramGuard and disables the active request for DRAM.
   */
  ~DramGuard() {
    if (!requestDramAccess(false)) {
      // TODO(stange): Change to FATAL_ERROR once this is guaranteed.
      LOGE("Unable to release DRAM access");
    }
  }
};

}  // namespace chre

#endif  // CHRE_PLATFORM_SHARED_DRAM_UTIL_H_
