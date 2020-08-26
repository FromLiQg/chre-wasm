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

#include "chre/target_platform/platform_cache_management.h"
#include "core/arm/generic/include/cache_management.h"
#include "core/arm/generic/include/memory_barrier.h"

namespace chre {

namespace {

void invalidateInstructionCache() {
  INSTRUCTION_CACHE_INVALIDATE();
  MB();
}

void cleanAndInvalidateDataCache() {
  CACHE_SET_WAY_FLUSH_AND_INVALIDATE_ALL();
  MB();
}

}  // anonymous namespace

void wipeSystemCaches() {
  cleanAndInvalidateDataCache();
  invalidateInstructionCache();
}

}  // namespace chre
