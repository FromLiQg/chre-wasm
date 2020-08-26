/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "chre/platform/platform_nanoapp.h"

#include <dlfcn.h>
#include <cinttypes>

#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre/platform/shared/memory.h"
#include "chre/platform/shared/nanoapp_dso_util.h"
#include "chre/platform/shared/nanoapp_loader.h"
#include "chre/util/macros.h"
#include "chre_api/chre/version.h"

namespace chre {
namespace {

const char kDefaultAppVersionString[] = "<undefined>";
size_t kDefaultAppVersionStringSize = ARRAY_SIZE(kDefaultAppVersionString);

}  // namespace

PlatformNanoapp::~PlatformNanoapp() {
  closeNanoapp();

  if (mAppBinary != nullptr) {
    forceDramAccess();
    memoryFreeDram(mAppBinary);
  }
}

bool PlatformNanoapp::start() {
  forceDramAccess();

  bool success = false;
  if (!openNanoapp()) {
    LOGE("Failed to open nanoapp");
  } else if (mAppInfo == nullptr) {
    LOGE("Null app info!");
  } else {
    success = mAppInfo->entryPoints.start();
  }
  return success;
}

void PlatformNanoapp::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                                  const void *eventData) {
  forceDramAccess();
  mAppInfo->entryPoints.handleEvent(senderInstanceId, eventType, eventData);
}

void PlatformNanoapp::end() {
  forceDramAccess();
  mAppInfo->entryPoints.end();
  closeNanoapp();
}

uint64_t PlatformNanoapp::getAppId() const {
  // TODO (karthikmb/stange): Ideally, we should store the metadata as SRAM
  // variables, to avoid bumping into DRAM for basic queries.
  forceDramAccess();
  return (mAppInfo != nullptr) ? mAppInfo->appId : mExpectedAppId;
}

uint32_t PlatformNanoapp::getAppVersion() const {
  forceDramAccess();
  return (mAppInfo != nullptr) ? mAppInfo->appVersion : mExpectedAppVersion;
}

const char *PlatformNanoapp::getAppName() const {
  forceDramAccess();
  return (mAppInfo != nullptr) ? mAppInfo->name : "Unknown";
}

uint32_t PlatformNanoapp::getTargetApiVersion() const {
  forceDramAccess();
  return (mAppInfo != nullptr) ? mAppInfo->targetApiVersion : 0;
}

bool PlatformNanoapp::isSystemNanoapp() const {
  forceDramAccess();
  return (mAppInfo != nullptr && mAppInfo->isSystemNanoapp);
}

void PlatformNanoapp::logStateToBuffer(DebugDumpWrapper &debugDump) const {
  if (mAppInfo != nullptr) {
    size_t versionLen = 0;
    const char *version = getAppVersionString(&versionLen);
    debugDump.print("%s (%s) @ build: %.*s", mAppInfo->name, mAppInfo->vendor,
                    versionLen, version);
  }
}

const char *PlatformNanoappBase::getAppVersionString(size_t *length) const {
  const char *versionString = kDefaultAppVersionString;
  *length = kDefaultAppVersionStringSize;

  size_t endOffset = 0;
  if (mAppUnstableId != nullptr) {
    size_t appVersionStringLength = strlen(mAppUnstableId);

    //! The unstable ID is expected to be in the format of
    //! <descriptor>=<build ID>@<more descriptors>. Use this expected layout
    //! knowledge to parse the string and only return the build ID portion that
    //! should be printed.
    size_t startOffset = SIZE_MAX;
    for (size_t i = 0; i < appVersionStringLength; i++) {
      size_t offset = i + 1;
      if (startOffset == SIZE_MAX && mAppUnstableId[i] == '=' &&
          offset < appVersionStringLength) {
        startOffset = offset;
      }

      if (mAppUnstableId[i] == '@') {
        endOffset = i;
        break;
      }
    }

    if (startOffset < endOffset) {
      versionString = &mAppUnstableId[startOffset];
      *length = endOffset - startOffset;
    }
  }

  return versionString;
}

bool PlatformNanoappBase::isLoaded() const {
  return (mIsStatic ||
          (mAppBinary != nullptr && mBytesLoaded == mAppBinaryLen) ||
          mDsoHandle != nullptr || mAppFilename != nullptr);
}

bool PlatformNanoappBase::isDramApp() const {
  // TODO: Determine if an app is in DRAM or not once nanoapps in SRAM
  // are supported.
  return true;
}

void PlatformNanoappBase::loadStatic(const struct chreNslNanoappInfo *appInfo) {
  CHRE_ASSERT(!isLoaded());
  mIsStatic = true;
  mAppInfo = appInfo;
}

bool PlatformNanoappBase::reserveBuffer(uint64_t appId, uint32_t appVersion,
                                        size_t appBinaryLen) {
  CHRE_ASSERT(!isLoaded());

  forceDramAccess();

  bool success = false;
  mAppBinary = memoryAllocDram(appBinaryLen);

  if (mAppBinary == nullptr) {
    LOG_OOM();
  } else {
    mExpectedAppId = appId;
    mExpectedAppVersion = appVersion;
    mAppBinaryLen = appBinaryLen;
    success = true;
  }

  return success;
}

bool PlatformNanoappBase::copyNanoappFragment(const void *buffer,
                                              size_t bufferLen) {
  CHRE_ASSERT(!isLoaded());

  forceDramAccess();

  bool success = true;

  if ((mBytesLoaded + bufferLen) > mAppBinaryLen) {
    LOGE("Overflow: cannot load %zu bytes to %zu/%zu nanoapp binary buffer",
         bufferLen, mBytesLoaded, mAppBinaryLen);
    success = false;
  } else {
    uint8_t *binaryBuffer = static_cast<uint8_t *>(mAppBinary) + mBytesLoaded;
    memcpy(binaryBuffer, buffer, bufferLen);
    mBytesLoaded += bufferLen;
  }

  return success;
}

bool PlatformNanoappBase::verifyNanoappInfo() {
  bool success = false;

  if (mDsoHandle == nullptr) {
    LOGE("No nanoapp info to verify");
  } else {
    mAppInfo = static_cast<const struct chreNslNanoappInfo *>(
        dlsym(mDsoHandle, CHRE_NSL_DSO_NANOAPP_INFO_SYMBOL_NAME));
    mAppUnstableId = static_cast<const char *>(
        dlsym(mDsoHandle, CHRE_NSL_DSO_NANOAPP_UNSTABLE_ID_SYMBOL_NAME));
    if (mAppInfo == nullptr) {
      LOGE("Failed to find app info symbol");
    } else if (mAppUnstableId == nullptr) {
      LOGE("Failed to find unstable ID symbol");
    } else {
      success = validateAppInfo(mExpectedAppId, mExpectedAppVersion, mAppInfo);
      if (!success) {
        mAppInfo = nullptr;
      } else {
        LOGI("Successfully loaded nanoapp: %s (0x%016" PRIx64
             ") version 0x%" PRIx32 " uimg %d system %d",
             mAppInfo->name, mAppInfo->appId, mAppInfo->appVersion,
             mAppInfo->isTcmNanoapp, mAppInfo->isSystemNanoapp);
      }
    }
  }
  return success;
}

bool PlatformNanoappBase::openNanoapp() {
  bool success = false;
  if (mIsStatic) {
    success = true;
  } else if (mAppBinary != nullptr) {
    if (mDsoHandle == nullptr) {
      mDsoHandle = dlopenbuf(mAppBinary);
      success = verifyNanoappInfo();
    } else {
      LOGE("Trying to reopen an existing buffer");
    }
  }

  if (!success) {
    closeNanoapp();
  }

  if (mAppBinary != nullptr) {
    memoryFreeDram(mAppBinary);
    mAppBinary = nullptr;
  }

  return success;
}

void PlatformNanoappBase::closeNanoapp() {
  if (mDsoHandle != nullptr) {
    forceDramAccess();
    mAppInfo = nullptr;
    if (dlclose(mDsoHandle) != 0) {
      LOGE("dlclose failed");
    }
    mDsoHandle = nullptr;
  }
}

}  // namespace chre
