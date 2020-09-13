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

#ifndef CHRE_PLATFORM_SHARED_MEMORY_H_
#define CHRE_PLATFORM_SHARED_MEMORY_H_

#include <cstddef>
#include <type_traits>

namespace chre {

/**
 * Aligned memory allocation using the lowest power heap available. The
 * semantics are the same as malloc.
 */
void *memoryAllocAligned(size_t alignment, size_t size);

/**
 * Aligned memory allocation specifically using the DRAM heap. The semantics are
 * the same as malloc.
 *
 * If DRAM or another large memory capacity region is not available, this will
 * allocate from the same memory region as memoryAlloc.
 */
void *memoryAllocDramAligned(size_t alignment, size_t size);

/**
 * Memory allocation specifically using the DRAM heap. The semantics are the
 * same as malloc.
 *
 * If DRAM or another large memory capacity region is not available, this will
 * allocate from the same memory region as memoryAlloc.
 */
void *memoryAllocDram(size_t size);

/**
 * Memory free from memory allocated using the DRAM heap. The semantics are the
 * same as free.
 *
 * If DRAM or another large memory capacity region is not available, this will
 * free from the same memory region as memoryFree.
 */
void memoryFreeDram(void *pointer);

/**
 * Ensures memory allocated through memoryAllocDram can be utilized. If memory
 * allocated through memoryAllocDram is always available, this method can be a
 * no-op.
 *
 * This must be support being invoked from multiple threads.
 */
void forceDramAccess();

/**
 * Removes CHRE's vote to keep DRAM accessible. This must only be called when
 * CHRE is idle.
 */
void removeDramAccessVote();

/**
 * Allocates memory in DRAM for an object of size T and constructs the object in
 * the newly allocated object by forwarding the provided parameters.
 */
template <typename T, typename... Args>
inline T *memoryAllocDram(Args &&... args) {
  auto *storage = static_cast<T *>(memoryAllocDram(sizeof(T)));
  if (storage != nullptr) {
    new (storage) T(std::forward<Args>(args)...);
  }

  return storage;
}

}  // namespace chre

#endif  // CHRE_PLATFORM_SHARED_MEMORY_H_
