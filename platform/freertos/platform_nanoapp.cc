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
#include "chre/platform/freertos/nanoapp_loader.h"
#include "chre/platform/shared/nanoapp_dso_util.h"

#include <cinttypes>

#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre_api/chre/version.h"

namespace chre {

PlatformNanoapp::~PlatformNanoapp() {}

bool PlatformNanoapp::start() {
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
  mAppInfo->entryPoints.handleEvent(senderInstanceId, eventType, eventData);
}

void PlatformNanoapp::end() {
  mAppInfo->entryPoints.end();
}

uint64_t PlatformNanoapp::getAppId() const {
  return (mAppInfo == nullptr) ? 0 : mAppInfo->appId;
}

uint32_t PlatformNanoapp::getAppVersion() const {
  return (mAppInfo == nullptr) ? 0 : mAppInfo->appVersion;
}

uint32_t PlatformNanoapp::getTargetApiVersion() const {
  return CHRE_API_VERSION;
}

bool PlatformNanoapp::isSystemNanoapp() const {
  return (mAppInfo != nullptr && mAppInfo->isSystemNanoapp);
}

void PlatformNanoapp::logStateToBuffer(
    DebugDumpWrapper & /* debugDump */) const {
  // TODO: stubbed out, Implement this.
}

bool PlatformNanoappBase::isLoaded() const {
  return (mIsStatic ||
          ((mAppBinary != nullptr) && (mAppBinaryLen != 0) &&
           (mBytesLoaded == mAppBinaryLen)) ||
          (mDsoHandle != nullptr));
}

void PlatformNanoappBase::loadStatic(const struct chreNslNanoappInfo *appInfo) {
  CHRE_ASSERT(!isLoaded());
  mIsStatic = true;
  mAppInfo = appInfo;
}

bool PlatformNanoappBase::reserveBuffer(uint64_t appId, uint32_t appVersion,
                                        size_t appBinaryLen) {
  CHRE_ASSERT(!isLoaded());

  bool success = false;
  // hack until malloc actually works
  // for prototyping hello world only
  static uint8_t tempBuffer[20000];
  static bool alreadyReserved = false;

  // TODO: usf plat chre memoryAlloc is being called here
  // mAppBinary = memoryAlloc(appBinaryLen);

  if (!alreadyReserved && (mAppBinary == nullptr)) {
    mAppBinary = (void *)(&tempBuffer[0]);
    mExpectedAppId = appId;
    mExpectedAppVersion = appVersion;
    mAppBinaryLen = appBinaryLen;
    success = true;
  } else {
    LOG_OOM();
  }

  return success;
}

bool PlatformNanoappBase::copyNanoappFragment(const void *buffer,
                                              size_t bufferLen) {
  CHRE_ASSERT(!isLoaded());

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
    LOGE("No nanoapp info to verify: %s", dlerror());
  } else {
    mAppInfo = static_cast<const struct chreNslNanoappInfo *>(
        dlsym(mDsoHandle, CHRE_NSL_DSO_NANOAPP_INFO_SYMBOL_NAME));
    if (mAppInfo == nullptr) {
      LOGE("Failed to find app info symbol: %s", dlerror());
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
      mDsoHandle = dlopenbuf(mAppBinary, mAppBinaryLen);
      if (mDsoHandle != nullptr) {
        mAppInfo = static_cast<struct chreNslNanoappInfo *>(
            dlsym(mDsoHandle, CHRE_NSL_DSO_NANOAPP_INFO_SYMBOL_NAME));

        if (mAppInfo != nullptr) {
          success =
              validateAppInfo(mExpectedAppId, mExpectedAppVersion, mAppInfo);
          if (!success) {
            mAppInfo = nullptr;
          } else {
            LOGI("Successfully loaded nanoapp: %s (0x%016" PRIx64
                 ") version 0x%" PRIx32 " uimg %d system %d",
                 mAppInfo->name, mAppInfo->appId, mAppInfo->appVersion,
                 mAppInfo->isTcmNanoapp, mAppInfo->isSystemNanoapp);
          }
        }
      } else {
        LOGE("Null DSO Handle!");
      }
    } else {
      LOGE("Trying to reopen an existing buffer");
    }
  }
  return success;
}

}  // namespace chre
