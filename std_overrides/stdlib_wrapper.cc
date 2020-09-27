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

/**
 * @file stdlib_wrapper.cc
 *
 * This file provides nanoapps with wrapper functions for standard library
 * functions in the standard library functions that are not supported alongside
 * CHRE, which either redirect the function to an analogous CHRE function, or
 * fail silently. The targeted usage of this is expected to be only for third-
 * party or generated functions, where changes to either the upstream third-
 * party code or the code generators might not be straightforward. It is
 * expected that the nanoapp developers are aware of the 'fail silently' clause
 * in the wrappers, and handle those cases appropriately.
 */

#include <stdio.h>
#include <stdlib.h>

#include <chre.h>
#include "chre/util/nanoapp/assert.h"

FILE *stderr = NULL;

void *malloc(size_t size) {
  return chreHeapAlloc(size);
}

void free(void *ptr) {
  chreHeapFree(ptr);
}

void *realloc(void * /*ptr*/, size_t /*newSize*/) {
  // realloc() is not supported, verify that there's no call to it!
  CHRE_ASSERT(false);
  return NULL;
}

void exit(int exitCode) {
  chreAbort(exitCode);
}

int fprintf(FILE * /*stream*/, const char * /*fmt*/, ...) {
  return 0;
}

size_t fwrite(const void * /*ptr*/, size_t /*size*/, size_t /*count*/,
              FILE * /*stream*/) {
  return 0;
}
