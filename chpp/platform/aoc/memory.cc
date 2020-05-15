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

#include "chpp/memory.h"
#include "chpp/macros.h"

#include "chre/platform/memory.h"
#include "string.h"

void *chppMalloc(const size_t size) {
  return chre::memoryAlloc(size);
}

void chppFree(void *ptr) {
  chre::memoryFree(ptr);
}

void *chppRealloc(void *oldPtr, const size_t newSize, const size_t oldSize) {
  if (newSize == 0) {
    chppFree(oldPtr);
    return nullptr;
  }
  if (newSize == oldSize) {
    return oldPtr;
  }

  void *ptr = chppMalloc(newSize);
  if (ptr != nullptr) {
    memcpy(ptr, oldPtr, newSize > oldSize ? oldSize : newSize);
    chppFree(oldPtr);
  }

  return ptr;
}
