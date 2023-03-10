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

// This file was automatically generated by chre_api_to_chpp.py
// Date: 2022-02-03 23:05:31 UTC
// Source: chre_api/include/chre_api/chre/wifi.h @ commit b5a92e457

// DO NOT modify this file directly, as those changes will be lost the next
// time the script is executed

#include "chpp/common/wifi_types.h"
#include "chpp/macros.h"
#include "chpp/memory.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Encoding (CHRE --> CHPP) size functions

//! @return number of bytes required to represent the given
//! chreWifiScanEvent along with the CHPP header as
//! struct ChppWifiScanEventWithHeader
static size_t chppWifiSizeOfScanEventFromChre(
    const struct chreWifiScanEvent *scanEvent) {
  size_t encodedSize = sizeof(struct ChppWifiScanEventWithHeader);
  encodedSize += scanEvent->scannedFreqListLen * sizeof(uint32_t);
  encodedSize += scanEvent->resultCount * sizeof(struct ChppWifiScanResult);
  return encodedSize;
}

//! @return number of bytes required to represent the given
//! chreWifiScanParams along with the CHPP header as
//! struct ChppWifiScanParamsWithHeader
static size_t chppWifiSizeOfScanParamsFromChre(
    const struct chreWifiScanParams *scanParams) {
  size_t encodedSize = sizeof(struct ChppWifiScanParamsWithHeader);
  encodedSize += scanParams->frequencyListLen * sizeof(uint32_t);
  encodedSize += scanParams->ssidListLen * sizeof(struct ChppWifiSsidListItem);
  return encodedSize;
}

//! @return number of bytes required to represent the given
//! chreWifiRangingEvent along with the CHPP header as
//! struct ChppWifiRangingEventWithHeader
static size_t chppWifiSizeOfRangingEventFromChre(
    const struct chreWifiRangingEvent *rangingEvent) {
  size_t encodedSize = sizeof(struct ChppWifiRangingEventWithHeader);
  encodedSize +=
      rangingEvent->resultCount * sizeof(struct ChppWifiRangingResult);
  return encodedSize;
}

//! @return number of bytes required to represent the given
//! chreWifiRangingParams along with the CHPP header as
//! struct ChppWifiRangingParamsWithHeader
static size_t chppWifiSizeOfRangingParamsFromChre(
    const struct chreWifiRangingParams *rangingParams) {
  size_t encodedSize = sizeof(struct ChppWifiRangingParamsWithHeader);
  encodedSize +=
      rangingParams->targetListLen * sizeof(struct ChppWifiRangingTarget);
  return encodedSize;
}

//! @return number of bytes required to represent the given
//! chreWifiNanSubscribeConfig along with the CHPP header as
//! struct ChppWifiNanSubscribeConfigWithHeader
static size_t chppWifiSizeOfNanSubscribeConfigFromChre(
    const struct chreWifiNanSubscribeConfig *nanSubscribeConfig) {
  size_t encodedSize = sizeof(struct ChppWifiNanSubscribeConfigWithHeader);
  if (nanSubscribeConfig->service != NULL) {
    encodedSize += strlen(nanSubscribeConfig->service) + 1;
  }
  encodedSize += nanSubscribeConfig->serviceSpecificInfoSize * sizeof(uint8_t);
  encodedSize += nanSubscribeConfig->matchFilterLength * sizeof(uint8_t);
  return encodedSize;
}

//! @return number of bytes required to represent the given
//! chreWifiNanDiscoveryEvent along with the CHPP header as
//! struct ChppWifiNanDiscoveryEventWithHeader
static size_t chppWifiSizeOfNanDiscoveryEventFromChre(
    const struct chreWifiNanDiscoveryEvent *nanDiscoveryEvent) {
  size_t encodedSize = sizeof(struct ChppWifiNanDiscoveryEventWithHeader);
  encodedSize += nanDiscoveryEvent->serviceSpecificInfoSize * sizeof(uint8_t);
  return encodedSize;
}

// Encoding (CHRE --> CHPP) conversion functions

static void chppWifiConvertScanResultFromChre(
    const struct chreWifiScanResult *in, struct ChppWifiScanResult *out) {
  out->ageMs = in->ageMs;
  out->capabilityInfo = in->capabilityInfo;
  out->ssidLen = in->ssidLen;
  memcpy(out->ssid, in->ssid, sizeof(out->ssid));
  memcpy(out->bssid, in->bssid, sizeof(out->bssid));
  out->flags = in->flags;
  out->rssi = in->rssi;
  out->band = in->band;
  out->primaryChannel = in->primaryChannel;
  out->centerFreqPrimary = in->centerFreqPrimary;
  out->centerFreqSecondary = in->centerFreqSecondary;
  out->channelWidth = in->channelWidth;
  out->securityMode = in->securityMode;
  out->radioChain = in->radioChain;
  out->rssiChain0 = in->rssiChain0;
  out->rssiChain1 = in->rssiChain1;
  memset(&out->reserved, 0, sizeof(out->reserved));
}

static void chppWifiConvertScanEventFromChre(const struct chreWifiScanEvent *in,
                                             struct ChppWifiScanEvent *out,
                                             uint8_t *payload,
                                             size_t payloadSize,
                                             uint16_t *vlaOffset) {
  out->version = CHRE_WIFI_SCAN_EVENT_VERSION;
  out->resultCount = in->resultCount;
  out->resultTotal = in->resultTotal;
  out->eventIndex = in->eventIndex;
  out->scanType = in->scanType;
  out->ssidSetSize = in->ssidSetSize;
  out->scannedFreqListLen = in->scannedFreqListLen;
  out->referenceTime = in->referenceTime;
  out->scannedFreqList.length =
      (uint16_t)(in->scannedFreqListLen * sizeof(uint32_t));
  CHPP_ASSERT((size_t)(*vlaOffset + out->scannedFreqList.length) <=
              payloadSize);
  if (out->scannedFreqList.length > 0 &&
      *vlaOffset + out->scannedFreqList.length <= payloadSize) {
    memcpy(&payload[*vlaOffset], in->scannedFreqList,
           in->scannedFreqListLen * sizeof(uint32_t));
    out->scannedFreqList.offset = *vlaOffset;
    *vlaOffset += out->scannedFreqList.length;
  } else {
    out->scannedFreqList.offset = 0;
  }

  struct ChppWifiScanResult *results =
      (struct ChppWifiScanResult *)&payload[*vlaOffset];
  out->results.length =
      (uint16_t)(in->resultCount * sizeof(struct ChppWifiScanResult));
  CHPP_ASSERT((size_t)(*vlaOffset + out->results.length) <= payloadSize);
  if (out->results.length > 0 &&
      *vlaOffset + out->results.length <= payloadSize) {
    for (size_t i = 0; i < in->resultCount; i++) {
      chppWifiConvertScanResultFromChre(&in->results[i], &results[i]);
    }
    out->results.offset = *vlaOffset;
    *vlaOffset += out->results.length;
  } else {
    out->results.offset = 0;
  }
  out->radioChainPref = in->radioChainPref;
}

static void chppWifiConvertSsidListItemFromChre(
    const struct chreWifiSsidListItem *in, struct ChppWifiSsidListItem *out) {
  out->ssidLen = in->ssidLen;
  memcpy(out->ssid, in->ssid, sizeof(out->ssid));
}

static void chppWifiConvertScanParamsFromChre(
    const struct chreWifiScanParams *in, struct ChppWifiScanParams *out,
    uint8_t *payload, size_t payloadSize, uint16_t *vlaOffset) {
  out->scanType = in->scanType;
  out->maxScanAgeMs = in->maxScanAgeMs;
  out->frequencyListLen = in->frequencyListLen;
  out->frequencyList.length =
      (uint16_t)(in->frequencyListLen * sizeof(uint32_t));
  CHPP_ASSERT((size_t)(*vlaOffset + out->frequencyList.length) <= payloadSize);
  if (out->frequencyList.length > 0 &&
      *vlaOffset + out->frequencyList.length <= payloadSize) {
    memcpy(&payload[*vlaOffset], in->frequencyList,
           in->frequencyListLen * sizeof(uint32_t));
    out->frequencyList.offset = *vlaOffset;
    *vlaOffset += out->frequencyList.length;
  } else {
    out->frequencyList.offset = 0;
  }
  out->ssidListLen = in->ssidListLen;

  struct ChppWifiSsidListItem *ssidList =
      (struct ChppWifiSsidListItem *)&payload[*vlaOffset];
  out->ssidList.length =
      (uint16_t)(in->ssidListLen * sizeof(struct ChppWifiSsidListItem));
  CHPP_ASSERT((size_t)(*vlaOffset + out->ssidList.length) <= payloadSize);
  if (out->ssidList.length > 0 &&
      *vlaOffset + out->ssidList.length <= payloadSize) {
    for (size_t i = 0; i < in->ssidListLen; i++) {
      chppWifiConvertSsidListItemFromChre(&in->ssidList[i], &ssidList[i]);
    }
    out->ssidList.offset = *vlaOffset;
    *vlaOffset += out->ssidList.length;
  } else {
    out->ssidList.offset = 0;
  }
  out->radioChainPref = in->radioChainPref;
  out->channelSet = in->channelSet;
}

static void chppWifiConvertLciFromChre(const struct chreWifiLci *in,
                                       struct ChppWifiLci *out) {
  out->latitude = in->latitude;
  out->longitude = in->longitude;
  out->altitude = in->altitude;
  out->latitudeUncertainty = in->latitudeUncertainty;
  out->longitudeUncertainty = in->longitudeUncertainty;
  out->altitudeType = in->altitudeType;
  out->altitudeUncertainty = in->altitudeUncertainty;
}

static void chppWifiConvertRangingResultFromChre(
    const struct chreWifiRangingResult *in, struct ChppWifiRangingResult *out) {
  out->timestamp = in->timestamp;
  memcpy(out->macAddress, in->macAddress, sizeof(out->macAddress));
  out->status = in->status;
  out->rssi = in->rssi;
  out->distance = in->distance;
  out->distanceStdDev = in->distanceStdDev;
  chppWifiConvertLciFromChre(&in->lci, &out->lci);
  out->flags = in->flags;
  memset(&out->reserved, 0, sizeof(out->reserved));
}

static void chppWifiConvertRangingEventFromChre(
    const struct chreWifiRangingEvent *in, struct ChppWifiRangingEvent *out,
    uint8_t *payload, size_t payloadSize, uint16_t *vlaOffset) {
  out->version = CHRE_WIFI_RANGING_EVENT_VERSION;
  out->resultCount = in->resultCount;
  memset(&out->reserved, 0, sizeof(out->reserved));

  struct ChppWifiRangingResult *results =
      (struct ChppWifiRangingResult *)&payload[*vlaOffset];
  out->results.length =
      (uint16_t)(in->resultCount * sizeof(struct ChppWifiRangingResult));
  CHPP_ASSERT((size_t)(*vlaOffset + out->results.length) <= payloadSize);
  if (out->results.length > 0 &&
      *vlaOffset + out->results.length <= payloadSize) {
    for (size_t i = 0; i < in->resultCount; i++) {
      chppWifiConvertRangingResultFromChre(&in->results[i], &results[i]);
    }
    out->results.offset = *vlaOffset;
    *vlaOffset += out->results.length;
  } else {
    out->results.offset = 0;
  }
}

static void chppWifiConvertRangingTargetFromChre(
    const struct chreWifiRangingTarget *in, struct ChppWifiRangingTarget *out) {
  memcpy(out->macAddress, in->macAddress, sizeof(out->macAddress));
  out->primaryChannel = in->primaryChannel;
  out->centerFreqPrimary = in->centerFreqPrimary;
  out->centerFreqSecondary = in->centerFreqSecondary;
  out->channelWidth = in->channelWidth;
  memset(&out->reserved, 0, sizeof(out->reserved));
}

static void chppWifiConvertRangingParamsFromChre(
    const struct chreWifiRangingParams *in, struct ChppWifiRangingParams *out,
    uint8_t *payload, size_t payloadSize, uint16_t *vlaOffset) {
  out->targetListLen = in->targetListLen;

  struct ChppWifiRangingTarget *targetList =
      (struct ChppWifiRangingTarget *)&payload[*vlaOffset];
  out->targetList.length =
      (uint16_t)(in->targetListLen * sizeof(struct ChppWifiRangingTarget));
  CHPP_ASSERT((size_t)(*vlaOffset + out->targetList.length) <= payloadSize);
  if (out->targetList.length > 0 &&
      *vlaOffset + out->targetList.length <= payloadSize) {
    for (size_t i = 0; i < in->targetListLen; i++) {
      chppWifiConvertRangingTargetFromChre(&in->targetList[i], &targetList[i]);
    }
    out->targetList.offset = *vlaOffset;
    *vlaOffset += out->targetList.length;
  } else {
    out->targetList.offset = 0;
  }
}

static void chppWifiConvertNanSubscribeConfigFromChre(
    const struct chreWifiNanSubscribeConfig *in,
    struct ChppWifiNanSubscribeConfig *out, uint8_t *payload,
    size_t payloadSize, uint16_t *vlaOffset) {
  out->subscribeType = in->subscribeType;
  if (in->service != NULL) {
    size_t strSize = strlen(in->service) + 1;
    memcpy(&payload[*vlaOffset], in->service, strSize);
    out->service.length = (uint16_t)(strSize);
    out->service.offset = *vlaOffset;
    *vlaOffset += out->service.length;
  } else {
    out->service.length = 0;
    out->service.offset = 0;
  }

  out->serviceSpecificInfo.length =
      (uint16_t)(in->serviceSpecificInfoSize * sizeof(uint8_t));
  CHPP_ASSERT((size_t)(*vlaOffset + out->serviceSpecificInfo.length) <=
              payloadSize);
  if (out->serviceSpecificInfo.length > 0 &&
      *vlaOffset + out->serviceSpecificInfo.length <= payloadSize) {
    memcpy(&payload[*vlaOffset], in->serviceSpecificInfo,
           in->serviceSpecificInfoSize * sizeof(uint8_t));
    out->serviceSpecificInfo.offset = *vlaOffset;
    *vlaOffset += out->serviceSpecificInfo.length;
  } else {
    out->serviceSpecificInfo.offset = 0;
  }
  out->serviceSpecificInfoSize = in->serviceSpecificInfoSize;
  out->matchFilter.length = (uint16_t)(in->matchFilterLength * sizeof(uint8_t));
  CHPP_ASSERT((size_t)(*vlaOffset + out->matchFilter.length) <= payloadSize);
  if (out->matchFilter.length > 0 &&
      *vlaOffset + out->matchFilter.length <= payloadSize) {
    memcpy(&payload[*vlaOffset], in->matchFilter,
           in->matchFilterLength * sizeof(uint8_t));
    out->matchFilter.offset = *vlaOffset;
    *vlaOffset += out->matchFilter.length;
  } else {
    out->matchFilter.offset = 0;
  }
  out->matchFilterLength = in->matchFilterLength;
}

static void chppWifiConvertNanDiscoveryEventFromChre(
    const struct chreWifiNanDiscoveryEvent *in,
    struct ChppWifiNanDiscoveryEvent *out, uint8_t *payload, size_t payloadSize,
    uint16_t *vlaOffset) {
  out->subscribeId = in->subscribeId;
  out->publishId = in->publishId;
  memcpy(out->publisherAddress, in->publisherAddress,
         sizeof(out->publisherAddress));
  out->serviceSpecificInfo.length =
      (uint16_t)(in->serviceSpecificInfoSize * sizeof(uint8_t));
  CHPP_ASSERT((size_t)(*vlaOffset + out->serviceSpecificInfo.length) <=
              payloadSize);
  if (out->serviceSpecificInfo.length > 0 &&
      *vlaOffset + out->serviceSpecificInfo.length <= payloadSize) {
    memcpy(&payload[*vlaOffset], in->serviceSpecificInfo,
           in->serviceSpecificInfoSize * sizeof(uint8_t));
    out->serviceSpecificInfo.offset = *vlaOffset;
    *vlaOffset += out->serviceSpecificInfo.length;
  } else {
    out->serviceSpecificInfo.offset = 0;
  }
  out->serviceSpecificInfoSize = in->serviceSpecificInfoSize;
}

static void chppWifiConvertNanSessionLostEventFromChre(
    const struct chreWifiNanSessionLostEvent *in,
    struct ChppWifiNanSessionLostEvent *out) {
  out->id = in->id;
  out->peerId = in->peerId;
}

static void chppWifiConvertNanSessionTerminatedEventFromChre(
    const struct chreWifiNanSessionTerminatedEvent *in,
    struct ChppWifiNanSessionTerminatedEvent *out) {
  out->id = in->id;
  out->reason = in->reason;
  memcpy(out->reserved, in->reserved, sizeof(out->reserved));
}

static void chppWifiConvertNanRangingParamsFromChre(
    const struct chreWifiNanRangingParams *in,
    struct ChppWifiNanRangingParams *out) {
  memcpy(out->macAddress, in->macAddress, sizeof(out->macAddress));
}

// Encoding (CHRE --> CHPP) top-level functions

bool chppWifiScanEventFromChre(const struct chreWifiScanEvent *in,
                               struct ChppWifiScanEventWithHeader **out,
                               size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize = chppWifiSizeOfScanEventFromChre(in);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    uint8_t *payload = (uint8_t *)&(*out)->payload;
    uint16_t vlaOffset = sizeof(struct ChppWifiScanEvent);
    chppWifiConvertScanEventFromChre(in, &(*out)->payload, payload, payloadSize,
                                     &vlaOffset);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

bool chppWifiScanParamsFromChre(const struct chreWifiScanParams *in,
                                struct ChppWifiScanParamsWithHeader **out,
                                size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize = chppWifiSizeOfScanParamsFromChre(in);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    uint8_t *payload = (uint8_t *)&(*out)->payload;
    uint16_t vlaOffset = sizeof(struct ChppWifiScanParams);
    chppWifiConvertScanParamsFromChre(in, &(*out)->payload, payload,
                                      payloadSize, &vlaOffset);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

bool chppWifiRangingEventFromChre(const struct chreWifiRangingEvent *in,
                                  struct ChppWifiRangingEventWithHeader **out,
                                  size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize = chppWifiSizeOfRangingEventFromChre(in);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    uint8_t *payload = (uint8_t *)&(*out)->payload;
    uint16_t vlaOffset = sizeof(struct ChppWifiRangingEvent);
    chppWifiConvertRangingEventFromChre(in, &(*out)->payload, payload,
                                        payloadSize, &vlaOffset);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

bool chppWifiRangingParamsFromChre(const struct chreWifiRangingParams *in,
                                   struct ChppWifiRangingParamsWithHeader **out,
                                   size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize = chppWifiSizeOfRangingParamsFromChre(in);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    uint8_t *payload = (uint8_t *)&(*out)->payload;
    uint16_t vlaOffset = sizeof(struct ChppWifiRangingParams);
    chppWifiConvertRangingParamsFromChre(in, &(*out)->payload, payload,
                                         payloadSize, &vlaOffset);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

bool chppWifiNanSubscribeConfigFromChre(
    const struct chreWifiNanSubscribeConfig *in,
    struct ChppWifiNanSubscribeConfigWithHeader **out, size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize = chppWifiSizeOfNanSubscribeConfigFromChre(in);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    uint8_t *payload = (uint8_t *)&(*out)->payload;
    uint16_t vlaOffset = sizeof(struct ChppWifiNanSubscribeConfig);
    chppWifiConvertNanSubscribeConfigFromChre(in, &(*out)->payload, payload,
                                              payloadSize, &vlaOffset);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

bool chppWifiNanDiscoveryEventFromChre(
    const struct chreWifiNanDiscoveryEvent *in,
    struct ChppWifiNanDiscoveryEventWithHeader **out, size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize = chppWifiSizeOfNanDiscoveryEventFromChre(in);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    uint8_t *payload = (uint8_t *)&(*out)->payload;
    uint16_t vlaOffset = sizeof(struct ChppWifiNanDiscoveryEvent);
    chppWifiConvertNanDiscoveryEventFromChre(in, &(*out)->payload, payload,
                                             payloadSize, &vlaOffset);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

bool chppWifiNanSessionLostEventFromChre(
    const struct chreWifiNanSessionLostEvent *in,
    struct ChppWifiNanSessionLostEventWithHeader **out, size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize = sizeof(struct ChppWifiNanSessionLostEventWithHeader);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    chppWifiConvertNanSessionLostEventFromChre(in, &(*out)->payload);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

bool chppWifiNanSessionTerminatedEventFromChre(
    const struct chreWifiNanSessionTerminatedEvent *in,
    struct ChppWifiNanSessionTerminatedEventWithHeader **out, size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize =
      sizeof(struct ChppWifiNanSessionTerminatedEventWithHeader);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    chppWifiConvertNanSessionTerminatedEventFromChre(in, &(*out)->payload);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

bool chppWifiNanRangingParamsFromChre(
    const struct chreWifiNanRangingParams *in,
    struct ChppWifiNanRangingParamsWithHeader **out, size_t *outSize) {
  CHPP_NOT_NULL(out);
  CHPP_NOT_NULL(outSize);

  size_t payloadSize = sizeof(struct ChppWifiNanRangingParamsWithHeader);
  *out = chppMalloc(payloadSize);
  if (*out != NULL) {
    chppWifiConvertNanRangingParamsFromChre(in, &(*out)->payload);
    *outSize = payloadSize;
    return true;
  }
  return false;
}

// Decoding (CHPP --> CHRE) conversion functions

static bool chppWifiConvertScanResultToChre(const struct ChppWifiScanResult *in,
                                            struct chreWifiScanResult *out) {
  out->ageMs = in->ageMs;
  out->capabilityInfo = in->capabilityInfo;
  out->ssidLen = in->ssidLen;
  memcpy(out->ssid, in->ssid, sizeof(out->ssid));
  memcpy(out->bssid, in->bssid, sizeof(out->bssid));
  out->flags = in->flags;
  out->rssi = in->rssi;
  out->band = in->band;
  out->primaryChannel = in->primaryChannel;
  out->centerFreqPrimary = in->centerFreqPrimary;
  out->centerFreqSecondary = in->centerFreqSecondary;
  out->channelWidth = in->channelWidth;
  out->securityMode = in->securityMode;
  out->radioChain = in->radioChain;
  out->rssiChain0 = in->rssiChain0;
  out->rssiChain1 = in->rssiChain1;
  memset(&out->reserved, 0, sizeof(out->reserved));

  return true;
}

static bool chppWifiConvertScanEventToChre(const struct ChppWifiScanEvent *in,
                                           struct chreWifiScanEvent *out,
                                           size_t inSize) {
  out->version = CHRE_WIFI_SCAN_EVENT_VERSION;
  out->resultCount = in->resultCount;
  out->resultTotal = in->resultTotal;
  out->eventIndex = in->eventIndex;
  out->scanType = in->scanType;
  out->ssidSetSize = in->ssidSetSize;
  out->scannedFreqListLen = in->scannedFreqListLen;
  out->referenceTime = in->referenceTime;

  if (in->scannedFreqList.length == 0) {
    out->scannedFreqList = NULL;
  } else {
    if (in->scannedFreqList.offset + in->scannedFreqList.length > inSize ||
        in->scannedFreqList.length !=
            in->scannedFreqListLen * sizeof(uint32_t)) {
      return false;
    }

    uint32_t *scannedFreqListOut =
        chppMalloc(in->scannedFreqListLen * sizeof(uint32_t));
    if (scannedFreqListOut == NULL) {
      return false;
    }

    memcpy(scannedFreqListOut,
           &((const uint8_t *)in)[in->scannedFreqList.offset],
           in->scannedFreqListLen * sizeof(uint32_t));
    out->scannedFreqList = scannedFreqListOut;
  }

  if (in->results.length == 0) {
    out->results = NULL;
  } else {
    if (in->results.offset + in->results.length > inSize ||
        in->results.length !=
            in->resultCount * sizeof(struct ChppWifiScanResult)) {
      return false;
    }

    const struct ChppWifiScanResult *resultsIn =
        (const struct ChppWifiScanResult *)&(
            (const uint8_t *)in)[in->results.offset];

    struct chreWifiScanResult *resultsOut =
        chppMalloc(in->resultCount * sizeof(struct chreWifiScanResult));
    if (resultsOut == NULL) {
      return false;
    }

    for (size_t i = 0; i < in->resultCount; i++) {
      if (!chppWifiConvertScanResultToChre(&resultsIn[i], &resultsOut[i])) {
        return false;
      }
    }
    out->results = resultsOut;
  }

  out->radioChainPref = in->radioChainPref;

  return true;
}

static bool chppWifiConvertSsidListItemToChre(
    const struct ChppWifiSsidListItem *in, struct chreWifiSsidListItem *out) {
  out->ssidLen = in->ssidLen;
  memcpy(out->ssid, in->ssid, sizeof(out->ssid));

  return true;
}

static bool chppWifiConvertScanParamsToChre(const struct ChppWifiScanParams *in,
                                            struct chreWifiScanParams *out,
                                            size_t inSize) {
  out->scanType = in->scanType;
  out->maxScanAgeMs = in->maxScanAgeMs;
  out->frequencyListLen = in->frequencyListLen;

  if (in->frequencyList.length == 0) {
    out->frequencyList = NULL;
  } else {
    if (in->frequencyList.offset + in->frequencyList.length > inSize ||
        in->frequencyList.length != in->frequencyListLen * sizeof(uint32_t)) {
      return false;
    }

    uint32_t *frequencyListOut =
        chppMalloc(in->frequencyListLen * sizeof(uint32_t));
    if (frequencyListOut == NULL) {
      return false;
    }

    memcpy(frequencyListOut, &((const uint8_t *)in)[in->frequencyList.offset],
           in->frequencyListLen * sizeof(uint32_t));
    out->frequencyList = frequencyListOut;
  }

  out->ssidListLen = in->ssidListLen;

  if (in->ssidList.length == 0) {
    out->ssidList = NULL;
  } else {
    if (in->ssidList.offset + in->ssidList.length > inSize ||
        in->ssidList.length !=
            in->ssidListLen * sizeof(struct ChppWifiSsidListItem)) {
      return false;
    }

    const struct ChppWifiSsidListItem *ssidListIn =
        (const struct ChppWifiSsidListItem *)&(
            (const uint8_t *)in)[in->ssidList.offset];

    struct chreWifiSsidListItem *ssidListOut =
        chppMalloc(in->ssidListLen * sizeof(struct chreWifiSsidListItem));
    if (ssidListOut == NULL) {
      return false;
    }

    for (size_t i = 0; i < in->ssidListLen; i++) {
      if (!chppWifiConvertSsidListItemToChre(&ssidListIn[i], &ssidListOut[i])) {
        return false;
      }
    }
    out->ssidList = ssidListOut;
  }

  out->radioChainPref = in->radioChainPref;
  out->channelSet = in->channelSet;

  return true;
}

static bool chppWifiConvertLciToChre(const struct ChppWifiLci *in,
                                     struct chreWifiLci *out) {
  out->latitude = in->latitude;
  out->longitude = in->longitude;
  out->altitude = in->altitude;
  out->latitudeUncertainty = in->latitudeUncertainty;
  out->longitudeUncertainty = in->longitudeUncertainty;
  out->altitudeType = in->altitudeType;
  out->altitudeUncertainty = in->altitudeUncertainty;

  return true;
}

static bool chppWifiConvertRangingResultToChre(
    const struct ChppWifiRangingResult *in, struct chreWifiRangingResult *out) {
  out->timestamp = in->timestamp;
  memcpy(out->macAddress, in->macAddress, sizeof(out->macAddress));
  out->status = in->status;
  out->rssi = in->rssi;
  out->distance = in->distance;
  out->distanceStdDev = in->distanceStdDev;
  if (!chppWifiConvertLciToChre(&in->lci, &out->lci)) {
    return false;
  }
  out->flags = in->flags;
  memset(&out->reserved, 0, sizeof(out->reserved));

  return true;
}

static bool chppWifiConvertRangingEventToChre(
    const struct ChppWifiRangingEvent *in, struct chreWifiRangingEvent *out,
    size_t inSize) {
  out->version = CHRE_WIFI_RANGING_EVENT_VERSION;
  out->resultCount = in->resultCount;
  memset(&out->reserved, 0, sizeof(out->reserved));

  if (in->results.length == 0) {
    out->results = NULL;
  } else {
    if (in->results.offset + in->results.length > inSize ||
        in->results.length !=
            in->resultCount * sizeof(struct ChppWifiRangingResult)) {
      return false;
    }

    const struct ChppWifiRangingResult *resultsIn =
        (const struct ChppWifiRangingResult *)&(
            (const uint8_t *)in)[in->results.offset];

    struct chreWifiRangingResult *resultsOut =
        chppMalloc(in->resultCount * sizeof(struct chreWifiRangingResult));
    if (resultsOut == NULL) {
      return false;
    }

    for (size_t i = 0; i < in->resultCount; i++) {
      if (!chppWifiConvertRangingResultToChre(&resultsIn[i], &resultsOut[i])) {
        return false;
      }
    }
    out->results = resultsOut;
  }

  return true;
}

static bool chppWifiConvertRangingTargetToChre(
    const struct ChppWifiRangingTarget *in, struct chreWifiRangingTarget *out) {
  memcpy(out->macAddress, in->macAddress, sizeof(out->macAddress));
  out->primaryChannel = in->primaryChannel;
  out->centerFreqPrimary = in->centerFreqPrimary;
  out->centerFreqSecondary = in->centerFreqSecondary;
  out->channelWidth = in->channelWidth;
  memset(&out->reserved, 0, sizeof(out->reserved));

  return true;
}

static bool chppWifiConvertRangingParamsToChre(
    const struct ChppWifiRangingParams *in, struct chreWifiRangingParams *out,
    size_t inSize) {
  out->targetListLen = in->targetListLen;

  if (in->targetList.length == 0) {
    out->targetList = NULL;
  } else {
    if (in->targetList.offset + in->targetList.length > inSize ||
        in->targetList.length !=
            in->targetListLen * sizeof(struct ChppWifiRangingTarget)) {
      return false;
    }

    const struct ChppWifiRangingTarget *targetListIn =
        (const struct ChppWifiRangingTarget *)&(
            (const uint8_t *)in)[in->targetList.offset];

    struct chreWifiRangingTarget *targetListOut =
        chppMalloc(in->targetListLen * sizeof(struct chreWifiRangingTarget));
    if (targetListOut == NULL) {
      return false;
    }

    for (size_t i = 0; i < in->targetListLen; i++) {
      if (!chppWifiConvertRangingTargetToChre(&targetListIn[i],
                                              &targetListOut[i])) {
        return false;
      }
    }
    out->targetList = targetListOut;
  }

  return true;
}

static bool chppWifiConvertNanSubscribeConfigToChre(
    const struct ChppWifiNanSubscribeConfig *in,
    struct chreWifiNanSubscribeConfig *out, size_t inSize) {
  out->subscribeType = in->subscribeType;

  if (in->service.length == 0) {
    out->service = NULL;
  } else {
    char *serviceOut = chppMalloc(in->service.length);
    if (serviceOut == NULL) {
      return false;
    }

    memcpy(serviceOut, &((const uint8_t *)in)[in->service.offset],
           in->service.length);
    out->service = serviceOut;
  }

  if (in->serviceSpecificInfo.length == 0) {
    out->serviceSpecificInfo = NULL;
  } else {
    if (in->serviceSpecificInfo.offset + in->serviceSpecificInfo.length >
            inSize ||
        in->serviceSpecificInfo.length !=
            in->serviceSpecificInfoSize * sizeof(uint8_t)) {
      return false;
    }

    uint8_t *serviceSpecificInfoOut =
        chppMalloc(in->serviceSpecificInfoSize * sizeof(uint8_t));
    if (serviceSpecificInfoOut == NULL) {
      return false;
    }

    memcpy(serviceSpecificInfoOut,
           &((const uint8_t *)in)[in->serviceSpecificInfo.offset],
           in->serviceSpecificInfoSize * sizeof(uint8_t));
    out->serviceSpecificInfo = serviceSpecificInfoOut;
  }

  out->serviceSpecificInfoSize = in->serviceSpecificInfoSize;

  if (in->matchFilter.length == 0) {
    out->matchFilter = NULL;
  } else {
    if (in->matchFilter.offset + in->matchFilter.length > inSize ||
        in->matchFilter.length != in->matchFilterLength * sizeof(uint8_t)) {
      return false;
    }

    uint8_t *matchFilterOut =
        chppMalloc(in->matchFilterLength * sizeof(uint8_t));
    if (matchFilterOut == NULL) {
      return false;
    }

    memcpy(matchFilterOut, &((const uint8_t *)in)[in->matchFilter.offset],
           in->matchFilterLength * sizeof(uint8_t));
    out->matchFilter = matchFilterOut;
  }

  out->matchFilterLength = in->matchFilterLength;

  return true;
}

static bool chppWifiConvertNanDiscoveryEventToChre(
    const struct ChppWifiNanDiscoveryEvent *in,
    struct chreWifiNanDiscoveryEvent *out, size_t inSize) {
  out->subscribeId = in->subscribeId;
  out->publishId = in->publishId;
  memcpy(out->publisherAddress, in->publisherAddress,
         sizeof(out->publisherAddress));

  if (in->serviceSpecificInfo.length == 0) {
    out->serviceSpecificInfo = NULL;
  } else {
    if (in->serviceSpecificInfo.offset + in->serviceSpecificInfo.length >
            inSize ||
        in->serviceSpecificInfo.length !=
            in->serviceSpecificInfoSize * sizeof(uint8_t)) {
      return false;
    }

    uint8_t *serviceSpecificInfoOut =
        chppMalloc(in->serviceSpecificInfoSize * sizeof(uint8_t));
    if (serviceSpecificInfoOut == NULL) {
      return false;
    }

    memcpy(serviceSpecificInfoOut,
           &((const uint8_t *)in)[in->serviceSpecificInfo.offset],
           in->serviceSpecificInfoSize * sizeof(uint8_t));
    out->serviceSpecificInfo = serviceSpecificInfoOut;
  }

  out->serviceSpecificInfoSize = in->serviceSpecificInfoSize;

  return true;
}

static bool chppWifiConvertNanSessionLostEventToChre(
    const struct ChppWifiNanSessionLostEvent *in,
    struct chreWifiNanSessionLostEvent *out) {
  out->id = in->id;
  out->peerId = in->peerId;

  return true;
}

static bool chppWifiConvertNanSessionTerminatedEventToChre(
    const struct ChppWifiNanSessionTerminatedEvent *in,
    struct chreWifiNanSessionTerminatedEvent *out) {
  out->id = in->id;
  out->reason = in->reason;
  memcpy(out->reserved, in->reserved, sizeof(out->reserved));

  return true;
}

static bool chppWifiConvertNanRangingParamsToChre(
    const struct ChppWifiNanRangingParams *in,
    struct chreWifiNanRangingParams *out) {
  memcpy(out->macAddress, in->macAddress, sizeof(out->macAddress));

  return true;
}

// Decoding (CHPP --> CHRE) top-level functions

struct chreWifiScanEvent *chppWifiScanEventToChre(
    const struct ChppWifiScanEvent *in, size_t inSize) {
  struct chreWifiScanEvent *out = NULL;

  if (inSize >= sizeof(struct ChppWifiScanEvent)) {
    out = chppMalloc(sizeof(struct chreWifiScanEvent));
    if (out != NULL) {
      if (!chppWifiConvertScanEventToChre(in, out, inSize)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
struct chreWifiScanParams *chppWifiScanParamsToChre(
    const struct ChppWifiScanParams *in, size_t inSize) {
  struct chreWifiScanParams *out = NULL;

  if (inSize >= sizeof(struct ChppWifiScanParams)) {
    out = chppMalloc(sizeof(struct chreWifiScanParams));
    if (out != NULL) {
      if (!chppWifiConvertScanParamsToChre(in, out, inSize)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
struct chreWifiRangingEvent *chppWifiRangingEventToChre(
    const struct ChppWifiRangingEvent *in, size_t inSize) {
  struct chreWifiRangingEvent *out = NULL;

  if (inSize >= sizeof(struct ChppWifiRangingEvent)) {
    out = chppMalloc(sizeof(struct chreWifiRangingEvent));
    if (out != NULL) {
      if (!chppWifiConvertRangingEventToChre(in, out, inSize)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
struct chreWifiRangingParams *chppWifiRangingParamsToChre(
    const struct ChppWifiRangingParams *in, size_t inSize) {
  struct chreWifiRangingParams *out = NULL;

  if (inSize >= sizeof(struct ChppWifiRangingParams)) {
    out = chppMalloc(sizeof(struct chreWifiRangingParams));
    if (out != NULL) {
      if (!chppWifiConvertRangingParamsToChre(in, out, inSize)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
struct chreWifiNanSubscribeConfig *chppWifiNanSubscribeConfigToChre(
    const struct ChppWifiNanSubscribeConfig *in, size_t inSize) {
  struct chreWifiNanSubscribeConfig *out = NULL;

  if (inSize >= sizeof(struct ChppWifiNanSubscribeConfig)) {
    out = chppMalloc(sizeof(struct chreWifiNanSubscribeConfig));
    if (out != NULL) {
      if (!chppWifiConvertNanSubscribeConfigToChre(in, out, inSize)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
struct chreWifiNanDiscoveryEvent *chppWifiNanDiscoveryEventToChre(
    const struct ChppWifiNanDiscoveryEvent *in, size_t inSize) {
  struct chreWifiNanDiscoveryEvent *out = NULL;

  if (inSize >= sizeof(struct ChppWifiNanDiscoveryEvent)) {
    out = chppMalloc(sizeof(struct chreWifiNanDiscoveryEvent));
    if (out != NULL) {
      if (!chppWifiConvertNanDiscoveryEventToChre(in, out, inSize)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
struct chreWifiNanSessionLostEvent *chppWifiNanSessionLostEventToChre(
    const struct ChppWifiNanSessionLostEvent *in, size_t inSize) {
  struct chreWifiNanSessionLostEvent *out = NULL;

  if (inSize >= sizeof(struct ChppWifiNanSessionLostEvent)) {
    out = chppMalloc(sizeof(struct chreWifiNanSessionLostEvent));
    if (out != NULL) {
      if (!chppWifiConvertNanSessionLostEventToChre(in, out)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
struct chreWifiNanSessionTerminatedEvent *
chppWifiNanSessionTerminatedEventToChre(
    const struct ChppWifiNanSessionTerminatedEvent *in, size_t inSize) {
  struct chreWifiNanSessionTerminatedEvent *out = NULL;

  if (inSize >= sizeof(struct ChppWifiNanSessionTerminatedEvent)) {
    out = chppMalloc(sizeof(struct chreWifiNanSessionTerminatedEvent));
    if (out != NULL) {
      if (!chppWifiConvertNanSessionTerminatedEventToChre(in, out)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
struct chreWifiNanRangingParams *chppWifiNanRangingParamsToChre(
    const struct ChppWifiNanRangingParams *in, size_t inSize) {
  struct chreWifiNanRangingParams *out = NULL;

  if (inSize >= sizeof(struct ChppWifiNanRangingParams)) {
    out = chppMalloc(sizeof(struct chreWifiNanRangingParams));
    if (out != NULL) {
      if (!chppWifiConvertNanRangingParamsToChre(in, out)) {
        CHPP_FREE_AND_NULLIFY(out);
      }
    }
  }

  return out;
}
