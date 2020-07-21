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

bool getWakeOutGpioPinNumber(ChppLinkType linkType, uint8_t *pinNumber) {
  bool success = true;
  switch (linkType) {
    // Refer to pin_configuration.cc for mapping
    case ChppLinkType::CHPP_LINK_TYPE_WIFI:
      *pinNumber = 43;
      break;
    case ChppLinkType::CHPP_LINK_TYPE_GNSS:
      *pinNumber = 86;
      break;
    case ChppLinkType::CHPP_LINK_TYPE_WWAN:
      // TODO: Needs to be defined
    default:
      CHPP_LOGE("No valid wake_out pin number found for link type %d",
                linkType);
      success = false;
      CHPP_ASSERT(false);
  }

  return success;
}

}  // anonymous namespace

UartLinkManager::UartLinkManager(ChppPlatformLinkParameters *params) {
  mUart = Environment::Instance()->UART(getUartMap(params->linkType));
  if (mUart == nullptr) {
    CHPP_LOGE("UART was null for link type %d", params->linkType);
  }
  CHPP_ASSERT(mUart != nullptr);

  uint8_t pinNumber;
  if (getWakeOutGpioPinNumber(params->linkType, &pinNumber)) {
    mWakeOutGpio = GPIOAoC(pinNumber);
    mWakeOutGpio->SetDirection(GPIO::DIRECTION::OUTPUT);
    mWakeOutGpio->Clear();
  }

  if (!isInitialized()) {
    CHPP_LOGE("Failed to initialize UartLinkManager for link type %d",
              params->linkType);
  }
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
  mWakeOutGpio->Set(true /* set */);

  // TODO: Wait for wake_in GPIO high

  if (hasTxPacket()) {
    int bytesTransmitted = mUart->Tx(mCurrentBuffer, mCurrentBufferLen);

    // Deassert wake_out as soon as data is transmitted, per specifications.
    mWakeOutGpio->Clear();

    if (static_cast<size_t>(bytesTransmitted) != mCurrentBufferLen) {
      CHPP_LOGE("Failed to transmit data");
    }
    // TODO: Inform CHPP transport layer of the transmission result
    clearTxPacket();
  } else {
    // TODO: Wait for pulse width requirement per specifications.
    mWakeOutGpio->Clear();
  }

  // TODO: Wait for wake_in GPIO low
}

bool UartLinkManager::isInitialized() const {
  return mUart != nullptr && mWakeOutGpio.has_value();
}

bool UartLinkManager::hasTxPacket() const {
  return mCurrentBuffer != nullptr && mUart != nullptr;
}

void UartLinkManager::clearTxPacket() {
  mCurrentBuffer = nullptr;
}

}  // namespace chpp
