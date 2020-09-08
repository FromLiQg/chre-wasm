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

#include "chpp/platform/chpp_init.h"

#include "chpp/app.h"
#include "chpp/platform/chpp_uart_link_manager.h"
#include "chpp/transport.h"
#include "chre/util/fixed_size_vector.h"
#include "chre/util/nested_data_ptr.h"
#include "environment.h"
#include "uart_map.h"

#include "task.h"

// Forward declaration
constexpr uint8_t chpp::UartLinkManager::kWifiWakeOutGpioPinNumber;
constexpr uint8_t chpp::UartLinkManager::kWifiWakeInGpiNumber;
constexpr uint8_t chpp::UartLinkManager::kGnssWakeOutGpioPinNumber;
constexpr uint8_t chpp::UartLinkManager::kGnssWakeInGpiNumber;
constexpr uint8_t chpp::UartLinkManager::kWwanWakeOutGpioPinNumber;
constexpr uint8_t chpp::UartLinkManager::kWwanWakeInGpiNumber;

using chre::FixedSizeVector;
using chre::NestedDataPtr;

namespace chpp {
namespace {

constexpr configSTACK_DEPTH_TYPE kChppTaskStackDepthWords = 0x400;

// Choose an appripriate priority to avoid initializing things too late.
// TODO: Configure priority in relation to CHRE based on end to end testing.
constexpr UBaseType_t kChppTaskPriority = tskIDLE_PRIORITY + 2;

// Structures to hold CHPP task related variables
constexpr size_t kChppLinkTotal = ChppLinkType::CHPP_LINK_TYPE_TOTAL;
TaskHandle_t gChppTaskHandleList[kChppLinkTotal];
ChppTransportState gChppTransportStateList[kChppLinkTotal];
ChppAppState gChppAppStateList[kChppLinkTotal];
FixedSizeVector<chpp::UartLinkManager, kChppLinkTotal> gManagerList;

struct ChppClientServiceSet getClientServiceSet(ChppLinkType type) {
  struct ChppClientServiceSet set = {};
  switch (type) {
    case ChppLinkType::CHPP_LINK_TYPE_WIFI:
      set.wifiClient = 1;
      break;
    case ChppLinkType::CHPP_LINK_TYPE_GNSS:
      set.gnssClient = 1;
      break;
    case ChppLinkType::CHPP_LINK_TYPE_WWAN:
      set.wwanClient = 1;
      break;
    default:
      LOGE("Invalid type %d to get client/service set", type);
      CHRE_ASSERT(false);
  }

  return set;
}

// Initializes the CHPP instance and runs the work thread.
void chppThreadEntry(void *context) {
  NestedDataPtr<ChppLinkType> type;
  type.dataPtr = context;

  struct ChppTransportState *transportState =
      &gChppTransportStateList[type.data];
  struct ChppAppState *appState = &gChppAppStateList[type.data];

  chppTransportInit(transportState, appState);
  chppAppInitWithClientServiceSet(appState, transportState,
                                  getClientServiceSet(type.data));

  chppWorkThreadStart(transportState);

  chppAppDeinit(appState);
  chppTransportDeinit(transportState);

  vTaskDelete(nullptr);
  gChppTaskHandleList[type.data] = nullptr;
}

BaseType_t startChppWorkThread(ChppLinkType type, const char *taskName) {
  struct ChppTransportState *transportState = &gChppTransportStateList[type];
  transportState->linkParams.uartLinkManager = &gManagerList[type];

  NestedDataPtr<ChppLinkType> linkType(type);
  return xTaskCreate(chppThreadEntry, taskName, kChppTaskStackDepthWords,
                     linkType.dataPtr /* args */, kChppTaskPriority,
                     &gChppTaskHandleList[type]);
}

void stopChppWorkThread(ChppLinkType type) {
  if (gChppTaskHandleList[type] != nullptr) {
    chppWorkThreadStop(&gChppTransportStateList[type]);
  }
}

}  // namespace

void init() {
  gManagerList.emplace_back(
      &gChppTransportStateList[ChppLinkType::CHPP_LINK_TYPE_WIFI],
      Environment::Instance()->UART(UART_MAP::UART_MAP_WIFI),
      chpp::UartLinkManager::kWifiWakeOutGpioPinNumber,
      chpp::UartLinkManager::kWifiWakeInGpiNumber);

  gManagerList.emplace_back(
      &gChppTransportStateList[ChppLinkType::CHPP_LINK_TYPE_GNSS],
      Environment::Instance()->UART(UART_MAP::UART_MAP_GNSS),
      chpp::UartLinkManager::kGnssWakeOutGpioPinNumber,
      chpp::UartLinkManager::kGnssWakeInGpiNumber);

  gManagerList.emplace_back(
      &gChppTransportStateList[ChppLinkType::CHPP_LINK_TYPE_WWAN],
      Environment::Instance()->UART(UART_MAP::UART_MAP_MODEM),
      chpp::UartLinkManager::kWwanWakeOutGpioPinNumber,
      chpp::UartLinkManager::kWwanWakeInGpiNumber);

  BaseType_t rc =
      startChppWorkThread(ChppLinkType::CHPP_LINK_TYPE_WIFI, "CHPP WIFI");
  CHRE_ASSERT(rc == pdPASS);

  rc = startChppWorkThread(ChppLinkType::CHPP_LINK_TYPE_GNSS, "CHPP GNSS");
  CHRE_ASSERT(rc == pdPASS);

  rc = startChppWorkThread(ChppLinkType::CHPP_LINK_TYPE_WWAN, "CHPP WWAN");
  CHRE_ASSERT(rc == pdPASS);
}

void deinit() {
  stopChppWorkThread(ChppLinkType::CHPP_LINK_TYPE_WIFI);
  stopChppWorkThread(ChppLinkType::CHPP_LINK_TYPE_GNSS);
  stopChppWorkThread(ChppLinkType::CHPP_LINK_TYPE_WWAN);
}

ChppAppState *getChppAppState(ChppLinkType linkType) {
  CHRE_ASSERT(linkType < ChppLinkType::CHPP_LINK_TYPE_TOTAL);
  return (linkType < ChppLinkType::CHPP_LINK_TYPE_TOTAL)
             ? &gChppAppStateList[linkType]
             : nullptr;
}

}  // namespace chpp
