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

#include "chpp/platform/chpp_uart_link_manager.h"

#include "chpp/macros.h"
#include "chpp/platform/log.h"
#include "efw/include/environment.h"
#include "uart_map.h"

namespace chpp {
namespace {

enum UART_MAP getUartMap(ChppLinkType linkType) {
  switch (linkType) {
    case ChppLinkType::CHPP_LINK_TYPE_WIFI:
      return UART_MAP::UART_MAP_WIFI;
    case ChppLinkType::CHPP_LINK_TYPE_GNSS:
      return UART_MAP::UART_MAP_GNSS;
    case ChppLinkType::CHPP_LINK_TYPE_WWAN:
      // TODO: Needs to be defined
    default:
      CHPP_ASSERT(false);
  }

  return UART_MAP::UART_MAP_TOT;
}

}  // anonymous namespace

UartLinkManager::UartLinkManager(ChppPlatformLinkParameters *params) {
  mUart = Environment::Instance()->UART(getUartMap(params->linkType));
  if (mUart == nullptr) {
    CHPP_LOGE("UART was null for link type %d", params->linkType);
  }
  CHPP_ASSERT(mUart != nullptr);
}

bool UartLinkManager::prepareTxPacket(uint8_t *buf, size_t len) {
  bool success = !hasTxPacket();
  if (!success) {
    CHPP_LOGE("Cannot prepare packet while one pending");
  } else {
    mCurrentBuffer = buf;
    mCurrentBufferLen = len;
  }
  return success;
}

void UartLinkManager::startTransaction() {
  // TODO: Add wake handshaking
  if (hasTxPacket()) {
    int bytesTransmitted = mUart->Tx(mCurrentBuffer, mCurrentBufferLen);
    if (static_cast<size_t>(bytesTransmitted) != mCurrentBufferLen) {
      CHPP_LOGE("Failed to transmit data");
    }
    // TODO: Inform CHPP transport layer of the transmission result
    clearTxPacket();
  }
}

bool UartLinkManager::hasTxPacket() const {
  return mCurrentBuffer != nullptr && mUart != nullptr;
}

void UartLinkManager::clearTxPacket() {
  mCurrentBuffer = nullptr;
}

}  // namespace chpp
