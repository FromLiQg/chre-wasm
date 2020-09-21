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

#include "chre/platform/aoc/memory.h"
#include "chre/platform/memory.h"
#include "chre/platform/mutex.h"
#include "chre/platform/shared/dram_vote_client.h"
#include "chre/platform/shared/pal_system_api.h"

#include "basic.h"
#include "heap.h"

#include <cstdlib>

//! Beginning of the CHRE SRAM heap.
extern uintptr_t __heap_chre_beg__;
//! End of the CHRE SRAM heap.
extern uintptr_t __heap_chre_end__;
//! Beginning of the CHRE DRAM heap.
extern uintptr_t __heap_chre_dram_beg__;
//! End of the CHRE DRAM heap.
extern uintptr_t __heap_chre_dram_end__;

namespace chre {
namespace {

//! Handle to the SRAM heap.
void *gSramHeap = nullptr;
//! Mutex used to synchronize access to the SRAM heap.
Mutex gSramMutex;
//! Handle to the DRAM heap.
void *gDramHeap = nullptr;
//! Mutex used to synchronize access to the DRAM heap.
Mutex gDramMutex;

enum ChreHeap { DRAM, SRAM };

//! Helper functions passed to EFW so it can lock when heap allocations / frees
//! are performed.
inline void LockMutex(void *mutex) {
  static_cast<Mutex *>(mutex)->lock();
}
inline void UnlockMutex(void *mutex) {
  static_cast<Mutex *>(mutex)->unlock();
}

void *GetSramHeap() {
  if (gSramHeap == nullptr) {
    auto heap_base = &__heap_chre_beg__;
    auto heap_size = PTR_OFFSET_GET(&__heap_chre_end__, heap_base);

    gSramHeap = HeapInit(heap_base, heap_size);
    CHRE_ASSERT(gSramHeap != nullptr);
    if (gSramHeap == nullptr) {
      return nullptr;
    } else {
      HeapLocking(gSramHeap, &gSramMutex, LockMutex, UnlockMutex);
    }
  }
  return gSramHeap;
}

void *GetDramHeap() {
  if (gDramHeap == nullptr) {
    auto heap_base = &__heap_chre_dram_beg__;
    auto heap_size = PTR_OFFSET_GET(&__heap_chre_dram_end__, heap_base);

    gDramHeap = HeapInit(heap_base, heap_size);
    CHRE_ASSERT(gDramHeap != nullptr);
    if (gDramHeap == nullptr) {
      return nullptr;
    } else {
      HeapLocking(gDramHeap, &gDramMutex, LockMutex, UnlockMutex);
    }
  }
  return gDramHeap;
}

void *GetHeap(ChreHeap heap) {
  if (heap == ChreHeap::SRAM) {
    return GetSramHeap();
  } else if (heap == ChreHeap::DRAM) {
    return GetDramHeap();
  } else {
    CHRE_ASSERT(false);
  }
  return nullptr;
}

bool IsInHeap(ChreHeap heap, const void *pointer) {
  void *handle = GetHeap(heap);
  bool found = false;
  if (handle != nullptr) {
    const struct HEAP_STATS *stats = HeapStats(handle);
    const uintptr_t heapBase = reinterpret_cast<uintptr_t>(stats->base);
    const uintptr_t castPointer = reinterpret_cast<uintptr_t>(pointer);
    found = castPointer >= heapBase && castPointer < (heapBase + stats->len);
  }

  return found;
}

}  // namespace

void *memoryAlloc(size_t size) {
  void *ptr = nullptr;
  void *handle = GetSramHeap();
  if (handle != nullptr) {
    ptr = HeapMalloc(handle, size);
    // Fall back to DRAM memory when SRAM memory is exhausted. Must exclude size
    // 0 as clients may not explicitly free memory of size 0, which may
    // mistakenly leave DRAM accessible
    if (ptr == nullptr && size != 0) {
      // Increment DRAM vote count to prevent system from powering down DRAM
      // while DRAM memory is in use.
      DramVoteClientSingleton::get()->incrementDramVoteCount();
      ptr = memoryAllocDram(size);

      // DRAM allocation failed too.
      if (ptr == nullptr) {
        DramVoteClientSingleton::get()->decrementDramVoteCount();
      }
    }
  }

  return ptr;
}

void *memoryAllocAligned(size_t alignment, size_t size) {
  void *ptr = nullptr;
  void *handle = GetSramHeap();

  if (handle != nullptr) {
    ptr = HeapAlignedAlloc(handle, alignment, size);
    // This method is currently only used for nanoapp loading so don't fall back
    // to DRAM or there can be power implications.
  }

  return ptr;
}

void *memoryAllocDramAligned(size_t alignment, size_t size) {
  CHRE_ASSERT_LOG(DramVoteClientSingleton::get()->isDramVoteActive(),
                  "DRAM allocation when not accessible");

  void *ptr = nullptr;
  void *handle = GetDramHeap();

  if (handle != nullptr) {
    ptr = HeapAlignedAlloc(handle, alignment, size);
  }

  return ptr;
}

void *memoryAllocDram(size_t size) {
  CHRE_ASSERT_LOG(DramVoteClientSingleton::get()->isDramVoteActive(),
                  "DRAM allocation when not accessible");

  void *ptr = nullptr;
  void *handle = GetDramHeap();
  if (handle != nullptr) {
    ptr = HeapMalloc(handle, size);
  }

  return ptr;
}

void memoryFree(void *pointer) {
  if (pointer != nullptr) {
    if (IsInHeap(ChreHeap::SRAM, pointer)) {
      HeapFree(GetHeap(ChreHeap::SRAM), pointer);
    } else {
      memoryFreeDram(pointer);
      DramVoteClientSingleton::get()->decrementDramVoteCount();
    }
  }
}

void memoryFreeDram(void *pointer) {
  CHRE_ASSERT_LOG(DramVoteClientSingleton::get()->isDramVoteActive(),
                  "DRAM freed when not accessible");

  if (pointer != nullptr) {
    bool inDramHeap = !IsInHeap(ChreHeap::DRAM, pointer);
    CHRE_ASSERT(inDramHeap);
    if (inDramHeap) {
      HeapFree(GetDramHeap(), pointer);
    }
  }
}

void forceDramAccess() {
  DramVoteClientSingleton::get()->voteDramAccess(true /* enabled */);
}

void removeDramAccessVote() {
  DramVoteClientSingleton::get()->voteDramAccess(false /* enabled */);
}

void *palSystemApiMemoryAlloc(size_t size) {
  return memoryAlloc(size);
}

void palSystemApiMemoryFree(void *pointer) {
  memoryFree(pointer);
}

}  // namespace chre
