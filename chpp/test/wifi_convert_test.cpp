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

#include <gtest/gtest.h>

#include "chpp/memory.h"
#include "chpp/services/wifi_types.h"

#include <cstring>

namespace {

void validateScanResult(const ChppWifiScanResult &chppAp,
                        const chreWifiScanResult &chreAp) {
  EXPECT_EQ(chppAp.ageMs, chreAp.ageMs);
  EXPECT_EQ(chppAp.capabilityInfo, chreAp.capabilityInfo);
  EXPECT_EQ(chppAp.ssidLen, chreAp.ssidLen);
  EXPECT_EQ(std::memcmp(chppAp.ssid, chreAp.ssid, sizeof(chppAp.ssid)), 0);
  EXPECT_EQ(std::memcmp(chppAp.bssid, chreAp.bssid, sizeof(chppAp.bssid)), 0);
  EXPECT_EQ(chppAp.flags, chreAp.flags);
  EXPECT_EQ(chppAp.rssi, chreAp.rssi);
  EXPECT_EQ(chppAp.band, chreAp.band);
  EXPECT_EQ(chppAp.primaryChannel, chreAp.primaryChannel);
  EXPECT_EQ(chppAp.centerFreqPrimary, chreAp.centerFreqPrimary);
  EXPECT_EQ(chppAp.centerFreqSecondary, chreAp.centerFreqSecondary);
  EXPECT_EQ(chppAp.channelWidth, chreAp.channelWidth);
  EXPECT_EQ(chppAp.securityMode, chreAp.securityMode);
  EXPECT_EQ(chppAp.radioChain, chreAp.radioChain);
  EXPECT_EQ(chppAp.rssiChain0, chreAp.rssiChain0);
  EXPECT_EQ(chppAp.rssiChain1, chreAp.rssiChain1);
  for (size_t i = 0; i < sizeof(chppAp.reserved); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(chppAp.reserved[i], 0);
  }
}

void validateScanEvent(const chreWifiScanEvent &chreEvent) {
  ChppWifiScanEvent *chppEvent = nullptr;
  size_t outputSize = 999;
  bool result = chppWifiScanEventFromChre(&chreEvent, &chppEvent, &outputSize);
  ASSERT_TRUE(result);
  ASSERT_NE(chppEvent, nullptr);

  size_t expectedSize = sizeof(ChppWifiScanEvent) +
                        chreEvent.scannedFreqListLen * sizeof(uint32_t) +
                        chreEvent.resultCount * sizeof(ChppWifiScanResult);
  EXPECT_EQ(outputSize, expectedSize);

  EXPECT_EQ(chppEvent->version, CHRE_WIFI_SCAN_EVENT_VERSION);
  EXPECT_EQ(chppEvent->resultCount, chreEvent.resultCount);
  EXPECT_EQ(chppEvent->resultTotal, chreEvent.resultTotal);
  EXPECT_EQ(chppEvent->resultCount, chreEvent.resultCount);
  EXPECT_EQ(chppEvent->eventIndex, chreEvent.eventIndex);
  EXPECT_EQ(chppEvent->scanType, chreEvent.scanType);
  EXPECT_EQ(chppEvent->ssidSetSize, chreEvent.ssidSetSize);
  EXPECT_EQ(chppEvent->scannedFreqListLen, chreEvent.scannedFreqListLen);
  EXPECT_EQ(chppEvent->referenceTime, chreEvent.referenceTime);
  EXPECT_EQ(chppEvent->radioChainPref, chreEvent.radioChainPref);

  uint16_t baseOffset = sizeof(ChppWifiScanEvent);
  if (chreEvent.scannedFreqListLen > 0) {
    EXPECT_EQ(chppEvent->scannedFreqList.offset, baseOffset);
    EXPECT_EQ(chppEvent->scannedFreqList.length,
              chppEvent->scannedFreqListLen * sizeof(uint32_t));
    baseOffset += chppEvent->scannedFreqList.length;

    const uint32_t *chppScannedFreqList =
        (const uint32_t *)((const uint8_t *)chppEvent +
                           chppEvent->scannedFreqList.offset);
    for (size_t i = 0; i < chppEvent->scannedFreqListLen; i++) {
      SCOPED_TRACE(i);
      EXPECT_EQ(chppScannedFreqList[i], chreEvent.scannedFreqList[i]);
    }
  } else {
    EXPECT_EQ(chppEvent->scannedFreqList.offset, 0);
    EXPECT_EQ(chppEvent->scannedFreqList.length, 0);
  }

  if (chreEvent.resultCount > 0) {
    EXPECT_EQ(chppEvent->results.offset, baseOffset);
    EXPECT_EQ(chppEvent->results.length,
              chppEvent->resultCount * sizeof(ChppWifiScanResult));
    baseOffset += chppEvent->results.length;

    const ChppWifiScanResult *chppAp =
        (const ChppWifiScanResult *)((const uint8_t *)chppEvent +
                                     chppEvent->results.offset);
    for (size_t i = 0; i < chppEvent->resultCount; i++) {
      SCOPED_TRACE(::testing::Message() << "Scan result index " << i);
      validateScanResult(chppAp[i], chreEvent.results[i]);
    }
  } else {
    EXPECT_EQ(chppEvent->results.offset, 0);
    EXPECT_EQ(chppEvent->results.length, 0);
  }

  chppFree(chppEvent);
}

void validateScanParams(const chreWifiScanParams &chreParams) {
  ChppWifiScanParams *chppParams = nullptr;
  size_t outputSize = 999;
  bool result =
      chppWifiScanParamsFromChre(&chreParams, &chppParams, &outputSize);
  ASSERT_TRUE(result);
  ASSERT_NE(chppParams, nullptr);

  size_t expectedSize = sizeof(ChppWifiScanParams) +
                        chreParams.frequencyListLen * sizeof(uint32_t) +
                        chreParams.ssidListLen * sizeof(ChppWifiSsidListItem);
  EXPECT_EQ(outputSize, expectedSize);

  EXPECT_EQ(chppParams->scanType, chreParams.scanType);
  EXPECT_EQ(chppParams->maxScanAgeMs, chreParams.maxScanAgeMs);
  EXPECT_EQ(chppParams->frequencyListLen, chreParams.frequencyListLen);
  EXPECT_EQ(chppParams->ssidListLen, chreParams.ssidListLen);
  EXPECT_EQ(chppParams->radioChainPref, chreParams.radioChainPref);

  uint16_t baseOffset = sizeof(ChppWifiScanParams);
  if (chreParams.frequencyListLen > 0) {
    EXPECT_EQ(chppParams->frequencyList.offset, baseOffset);
    EXPECT_EQ(chppParams->frequencyList.length,
              chppParams->frequencyListLen * sizeof(uint32_t));
    baseOffset += chppParams->frequencyList.length;

    const uint32_t *chppFrequencyList =
        (const uint32_t *)((const uint8_t *)chppParams +
                           chppParams->frequencyList.offset);
    for (size_t i = 0; i < chppParams->frequencyListLen; i++) {
      SCOPED_TRACE(i);
      EXPECT_EQ(chppFrequencyList[i], chreParams.frequencyList[i]);
    }
  } else {
    EXPECT_EQ(chppParams->frequencyList.offset, 0);
    EXPECT_EQ(chppParams->frequencyList.length, 0);
  }

  if (chreParams.ssidListLen > 0) {
    EXPECT_EQ(chppParams->ssidList.offset, baseOffset);
    EXPECT_EQ(chppParams->ssidList.length,
              chppParams->ssidListLen * sizeof(ChppWifiSsidListItem));
    baseOffset += chppParams->ssidList.length;

    const ChppWifiSsidListItem *chppSsidList =
        (const ChppWifiSsidListItem *)((const uint8_t *)chppParams +
                                       chppParams->ssidList.offset);
    for (size_t i = 0; i < chppParams->ssidListLen; i++) {
      SCOPED_TRACE(i);
      EXPECT_EQ(chppSsidList[i].ssidLen, chreParams.ssidList[i].ssidLen);
      EXPECT_EQ(std::memcmp(chppSsidList[i].ssid, chreParams.ssidList[i].ssid,
                            sizeof(chppSsidList[i].ssid)),
                0);
    }
  } else {
    EXPECT_EQ(chppParams->ssidList.offset, 0);
    EXPECT_EQ(chppParams->ssidList.length, 0);
  }

  chppFree(chppParams);
}

}  // anonymous namespace

TEST(WifiConvert, EmptyScanResult) {
  const chreWifiScanEvent chreEvent = {
      .version = 200,  // ignored
      .resultCount = 0,
      .resultTotal = 0,
      .eventIndex = 0,
      .scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE_PLUS_PASSIVE_DFS,
      .ssidSetSize = 2,
      .scannedFreqListLen = 0,
      .referenceTime = 1234,
      .scannedFreqList = nullptr,
      .results = nullptr,
      .radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_HIGH_ACCURACY,
  };

  validateScanEvent(chreEvent);
}

TEST(WifiConvert, SingleResult) {
  // clang-format off
  const chreWifiScanResult chreAp = {
      .ageMs = 11,
      .capabilityInfo = 22,
      .ssidLen = 4,
      .ssid = {'a', 'b', 'c', 'd',},
      .bssid = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff},
      .flags = CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER,
      .rssi = -37,
      .band = CHRE_WIFI_BAND_2_4_GHZ,
      .primaryChannel = 2437,
      .centerFreqPrimary = 2442,
      .centerFreqSecondary = 2447,
      .channelWidth = CHRE_WIFI_CHANNEL_WIDTH_80_MHZ,
      .securityMode = CHRE_WIFI_SECURITY_MODE_PSK,
      .radioChain = CHRE_WIFI_RADIO_CHAIN_0 | CHRE_WIFI_RADIO_CHAIN_1,
      .rssiChain0 = -37,
      .rssiChain1 = -42,
  };
  const chreWifiScanEvent chreEvent = {
      .version = 200,  // ignored
      .resultCount = 1,
      .resultTotal = 5,
      .eventIndex = 2,
      .scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE,
      .ssidSetSize = 0,
      .scannedFreqListLen = 0,
      .referenceTime = 12345,
      .scannedFreqList = nullptr,
      .results = &chreAp,
      .radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT,
  };
  // clang-format on

  validateScanEvent(chreEvent);
}

TEST(WifiConvert, TwoResultsWithFreqList) {
  // clang-format off
  const chreWifiScanResult chreAps[] = {
  {
      .ageMs = 11,
      .capabilityInfo = 22,
      .ssidLen = 4,
      .ssid = {'a', 'b', 'c', 'd',},
      .bssid = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff},
      .flags = CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER,
      .rssi = -37,
      .band = CHRE_WIFI_BAND_2_4_GHZ,
      .primaryChannel = 2437,
      .centerFreqPrimary = 2442,
      .centerFreqSecondary = 2447,
      .channelWidth = CHRE_WIFI_CHANNEL_WIDTH_80_MHZ,
      .securityMode = CHRE_WIFI_SECURITY_MODE_PSK,
      .radioChain = CHRE_WIFI_RADIO_CHAIN_0 | CHRE_WIFI_RADIO_CHAIN_1,
      .rssiChain0 = -37,
      .rssiChain1 = -42,
  },
  {
      .ageMs = 4325,
      .capabilityInfo = 37,
      .ssidLen = 2,
      .ssid = {'h', 'i',},
      .bssid = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45},
      .flags = CHRE_WIFI_SCAN_RESULT_FLAGS_VHT_OPS_PRESENT,
      .rssi = -52,
      .band = CHRE_WIFI_BAND_5_GHZ,
      .primaryChannel = 9999,
      .centerFreqPrimary = 8888,
      .centerFreqSecondary = 7777,
      .channelWidth = CHRE_WIFI_CHANNEL_WIDTH_160_MHZ,
      .securityMode = CHRE_WIFI_SECURITY_MODE_PSK | CHRE_WIFI_SECURITY_MODE_EAP,
      .radioChain = CHRE_WIFI_RADIO_CHAIN_0,
      .rssiChain0 = -37,
      .rssiChain1 = 0,
  }};
  const uint32_t freqList[] = {1234, 2345, 3456};
  const chreWifiScanEvent chreEvent = {
      .version = 200,  // ignored
      .resultCount = 2,
      .resultTotal = 3,
      .eventIndex = 1,
      .scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE,
      .ssidSetSize = 10,
      .scannedFreqListLen = 3,
      .referenceTime = 56789,
      .scannedFreqList = freqList,
      .results = chreAps,
      .radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_LOW_POWER,
  };
  // clang-format on

  validateScanEvent(chreEvent);
}

TEST(WifiConvert, DefaultScanParams) {
  // From chreWifiRequestScanAsyncDefault
  struct chreWifiScanParams params = {};
  params.scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE;
  params.maxScanAgeMs = 5000;  // 5 seconds
  params.frequencyListLen = 0;
  params.ssidListLen = 0;
  params.radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT;

  validateScanParams(params);
}

TEST(WifiConvert, ScanParamsWithFreqList) {
  uint32_t freqList[] = {1234, 3456};
  struct chreWifiScanParams chreParams = {
      .scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE_PLUS_PASSIVE_DFS,
      .maxScanAgeMs = 9999,
      .frequencyListLen = 2,
      .frequencyList = freqList,
      .ssidListLen = 0,
      .ssidList = nullptr,
      .radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_LOW_POWER,
  };

  validateScanParams(chreParams);
}

TEST(WifiConvert, ScanParamsWithSsidList) {
  // clang-format off
  chreWifiSsidListItem ssidList[] = {
      {.ssidLen = 4, .ssid = {0xde, 0xad, 0xbe, 0xef}},
      {.ssidLen = 2, .ssid = {':', ')'}}
  };
  // clang-format on
  struct chreWifiScanParams chreParams = {
      .scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE_PLUS_PASSIVE_DFS,
      .maxScanAgeMs = 9999,
      .frequencyListLen = 0,
      .frequencyList = nullptr,
      .ssidListLen = 2,
      .ssidList = ssidList,
      .radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_LOW_POWER,
  };

  validateScanParams(chreParams);
}

TEST(WifiConvert, ScanParamsWithBothLists) {
  uint32_t freqList[] = {1234, 3456, 5678};
  // clang-format off
  chreWifiSsidListItem ssidList[] = {
      {.ssidLen = 4, .ssid = {0xde, 0xad, 0xbe, 0xef}},
      {.ssidLen = 3, .ssid = {':', '-', ')'}}
  };
  // clang-format on
  struct chreWifiScanParams chreParams = {
      .scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE_PLUS_PASSIVE_DFS,
      .maxScanAgeMs = 9999,
      .frequencyListLen = 3,
      .frequencyList = freqList,
      .ssidListLen = 2,
      .ssidList = ssidList,
      .radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_LOW_POWER,
  };

  validateScanParams(chreParams);
}
