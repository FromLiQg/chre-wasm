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

#include "chre/platform/freertos/dlfcn.h"

#include "chre/platform/assert.h"
#include "chre/platform/freertos/nanoapp_loader.h"
#include "chre/platform/memory.h"
#include "chre/util/unique_ptr.h"

namespace chre {

void *dlopenbuf(void *elfBinary) {
  return NanoappLoader::create(elfBinary);
}

void *dlsym(void *handle, const char *symbol) {
  LOGV("Attempting to find %s", symbol);

  void *resolvedSymbol = nullptr;
  auto *loader = reinterpret_cast<NanoappLoader *>(handle);
  if (loader != nullptr) {
    resolvedSymbol = loader->findSymbolByName(symbol);
    LOGV("Found symbol at %p", resolvedSymbol);
  }
  return resolvedSymbol;
}

int dlclose(void *handle) {
  int rv = -1;
  UniquePtr<NanoappLoader> loader = static_cast<NanoappLoader *>(handle);

  if (!loader.isNull()) {
    loader->deinit();
    rv = 0;
  }

  return rv;
}

const char *dlerror() {
  return "Shared library load failed";
}

}  // namespace chre
