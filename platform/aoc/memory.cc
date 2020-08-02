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
#include "chre/platform/shared/pal_system_api.h"

#include "basic.h"
#include "heap.h"
#include "sysmem.h"

#include <cstdlib>

// TODO(b/152442881): Remove printf statements and replace with CHRE_ASSERT
// when things are considered more stable.

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

//! SRAM heap handle.
void *gSramHeap = nullptr;
//! SRAM heap mutex.
Mutex gSramMutex;
//! DRAM handle.
void *gDramHeap = nullptr;
//! DRAM mutex.
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
      printf("CHRE: Failed to initialize SRAM heap\n");
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
      printf("CHRE: Failed to initialize DRAM heap\n");
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
    printf("CHRE: GetHeap given invalid heap\n");
  }
  return nullptr;
}

bool IsInHeap(ChreHeap heap, const void *pointer) {
  void *handle = GetHeap(heap);
  bool isDramHeap = (heap == ChreHeap::DRAM);
  if (isDramHeap) {
    CHRE_ASSERT(requestDramAccess(true /*enabled*/));
  }
  bool found = false;
  if (handle != nullptr) {
    const struct HEAP_STATS *stats = HeapStats(handle);
    const uintptr_t heapBase = reinterpret_cast<uintptr_t>(stats->base);
    const uintptr_t castPointer = reinterpret_cast<uintptr_t>(pointer);
    found = castPointer >= heapBase && castPointer < (heapBase + stats->len);
  }

  if (isDramHeap) {
    requestDramAccess(false /*enabled*/);
  }

  return found;
}

void *GetHeapFromPointer(const void *pointer) {
  if (IsInHeap(ChreHeap::SRAM, pointer)) {
    return GetSramHeap();
  }

  if (IsInHeap(ChreHeap::DRAM, pointer)) {
    return GetDramHeap();
  }

  return nullptr;
}

}  // namespace

void *memoryAlloc(size_t size) {
  void *ptr = nullptr;
  void *handle = GetSramHeap();
  if (handle != nullptr) {
    ptr = HeapMalloc(handle, size);
    if (ptr == nullptr) {
      printf("CHRE: Failed to allocate memory in SRAM heap\n");
      // TODO: Ideally if we fail to allocate enough memory in SRAM, we should
      // try and allocate in DRAM. Revisit this once all nanoapps have been
      // loaded, and the full repercussions of tiered memory usage have been
      // hashed out.
    }
  }

  return ptr;
}

void *memoryAllocDramAligned(size_t alignment, size_t size) {
  void *ptr = nullptr;
  void *handle = GetDramHeap();

  if (handle != nullptr) {
    ptr = HeapAlignedAlloc(handle, alignment, size);
    if (ptr == nullptr) {
      printf("CHRE: Failed to allocate memory in DRAM heap\n");
    }
  }

  return ptr;
}

void *memoryAllocDram(size_t size) {
  void *ptr = nullptr;
  void *handle = GetDramHeap();
  if (handle != nullptr) {
    ptr = HeapMalloc(handle, size);
    if (ptr == nullptr) {
      printf("CHRE: Failed to allocate memory in DRAM heap\n");
    }
  }

  return ptr;
}

void memoryFree(void *pointer) {
  if (pointer != nullptr) {
    if (!IsInHeap(ChreHeap::DRAM, pointer)) {
      void *handle = GetHeapFromPointer(pointer);
      if (handle == nullptr) {
        printf("CHRE: Tried to free memory from a non-CHRE heap\n");
      } else {
        HeapFree(handle, pointer);
      }
    } else {
      printf("Please call memoryFreeDram to free DRAM heap mem\n");
    }
  }
}

void memoryFreeDram(void *pointer) {
  if (!IsInHeap(ChreHeap::DRAM, pointer)) {
    printf(
        "CHRE: Tried to free memory not in DRAM heap or DRAM heap not \
            initialized\n");
  } else {
    HeapFree(GetDramHeap(), pointer);
  }
}

bool requestDramAccess(bool enabled) {
  int rc;
  if (enabled) {
    rc = SysMem::Instance()->MemoryRequest(SysMem::MIF, true);
  } else {
    rc = SysMem::Instance()->MemoryRelease(SysMem::MIF);
  }

  if (rc != 0) {
    LOGE("Unable to change DRAM access to %d with error code %d", enabled, rc);
  }

  return rc == 0;
}

void *palSystemApiMemoryAlloc(size_t size) {
  return memoryAlloc(size);
}

void palSystemApiMemoryFree(void *pointer) {
  memoryFree(pointer);
}

}  // namespace chre
