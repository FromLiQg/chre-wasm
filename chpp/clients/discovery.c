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

#include "chpp/clients/discovery.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "chpp/app.h"
#include "chpp/common/discovery.h"
#include "chpp/macros.h"
#include "chpp/memory.h"
#include "chpp/platform/log.h"
#include "chpp/transport.h"

/************************************************
 *  Prototypes
 ***********************************************/

static inline bool chppIsClientCompatibleWithService(
    const struct ChppClientDescriptor *client,
    struct ChppServiceDescriptor *service);
static uint8_t chppFindMatchingClient(struct ChppAppState *context,
                                      struct ChppServiceDescriptor *service);
static void chppDiscoveryProcessDiscoverAll(struct ChppAppState *context,
                                            const uint8_t *buf, size_t len);

/************************************************
 *  Private Functions
 ***********************************************/

/**
 * Determines if a client is compatible with a service. Compatibility
 * requirements are:
 * 1. UUIDs must match
 * 2. Major version numbers must match
 *
 * @param client ChppClientDescriptor of client.
 * @param service ChppServiceDescriptor of service.
 *
 * @param return True if compatible.
 */
static inline bool chppIsClientCompatibleWithService(
    const struct ChppClientDescriptor *client,
    struct ChppServiceDescriptor *service) {
  return (memcmp(client->uuid, service->uuid, CHPP_SERVICE_UUID_LEN) == 0 &&
          client->version.major == service->version.major);
}

/**
 * Attempts to match a registered client to a (discovered) service, responding
 * with either the client index or CHPP_CLIENT_INDEX_NONE if it fails.
 *
 * @param context Maintains status for each app layer instance.
 * @param service ChppServiceDescriptor of service.
 *
 * @param return Index of client matching the service, or CHPP_CLIENT_INDEX_NONE
 * if there is none.
 */
static uint8_t chppFindMatchingClient(struct ChppAppState *context,
                                      struct ChppServiceDescriptor *service) {
  uint8_t result = CHPP_CLIENT_INDEX_NONE;

  for (size_t i = 0; i < context->registeredClientCount; i++) {
    if (chppIsClientCompatibleWithService(
            &context->registeredClients[i]->descriptor, service)) {
      result = i;
      break;
    }
  }

  return result;
}

/**
 * Processes the Discover All Services response
 * (CHPP_DISCOVERY_COMMAND_DISCOVER_ALL).
 *
 * @param context Maintains status for each app layer instance.
 * @param buf Input (request) datagram. Cannot be null.
 * @param len Length of input data in bytes.
 */
static void chppDiscoveryProcessDiscoverAll(struct ChppAppState *context,
                                            const uint8_t *buf, size_t len) {
  CHPP_DEBUG_ASSERT(len >= sizeof(struct ChppAppHeader));

  struct ChppDiscoveryResponse *response = (struct ChppDiscoveryResponse *)buf;
  size_t servicesLen = len - sizeof(struct ChppAppHeader);
  uint8_t serviceCount = servicesLen / sizeof(struct ChppServiceDescriptor);

  if (servicesLen != serviceCount * sizeof(struct ChppServiceDescriptor)) {
    // Incomplete service list
    CHPP_LOGE(
        "Service descriptors length length=%zu is invalid for a service count "
        "= %" PRIu8 " and descriptor length = %zu",
        servicesLen, serviceCount, sizeof(struct ChppServiceDescriptor));
    CHPP_DEBUG_ASSERT(false);
  }

  if (serviceCount >= CHPP_MAX_DISCOVERED_SERVICES) {
    CHPP_LOGE("Discovered service count = %" PRIu8
              " larger than CHPP_MAX_DISCOVERED_SERVICES = %" PRIu8,
              serviceCount, CHPP_MAX_DISCOVERED_SERVICES);
    CHPP_DEBUG_ASSERT(false);
  }

  CHPP_LOGI("Attempting to match %" PRIu8 " registered clients and %" PRIu8
            " discovered services",
            context->registeredClientCount, serviceCount);

  uint8_t matchedClients = 0;
  for (uint8_t i = 0; i < MIN(serviceCount, CHPP_MAX_DISCOVERED_SERVICES);
       i++) {
    // Update lookup table
    context->clientIndexOfServiceIndex[i] =
        chppFindMatchingClient(context, &response->services[i]);

    char uuidText[CHPP_SERVICE_UUID_STRING_LEN];
    chppUuidToStr(response->services[i].uuid, uuidText);

    if (context->clientIndexOfServiceIndex[i] == CHPP_CLIENT_INDEX_NONE) {
      CHPP_LOGI("No matching client found for service on handle %" PRIu8
                " with name=%s, UUID=%s, version=%" PRIu8 ".%" PRIu8
                ".%" PRIu16,
                CHPP_SERVICE_HANDLE_OF_INDEX(i), response->services[i].name,
                uuidText, response->services[i].version.major,
                response->services[i].version.minor,
                response->services[i].version.patch);

    } else {
      CHPP_LOGI(
          "Client # %" PRIu8 " matched to service on handle %" PRIu8
          " with name=%s, UUID=%s. "
          "client version=%" PRIu8 ".%" PRIu8 ".%" PRIu16
          ", service version=%" PRIu8 ".%" PRIu8 ".%" PRIu16,
          context->clientIndexOfServiceIndex[i],
          CHPP_SERVICE_HANDLE_OF_INDEX(i), response->services[i].name, uuidText,
          context->registeredClients[context->clientIndexOfServiceIndex[i]]
              ->descriptor.version.major,
          context->registeredClients[context->clientIndexOfServiceIndex[i]]
              ->descriptor.version.minor,
          context->registeredClients[context->clientIndexOfServiceIndex[i]]
              ->descriptor.version.patch,
          response->services[i].version.major,
          response->services[i].version.minor,
          response->services[i].version.patch);

      // Initialize client
      if (context->registeredClients[0]->initFunctionPtr(
              context, CHPP_SERVICE_HANDLE_OF_INDEX(i),
              response->services[i].version) == false) {
        CHPP_LOGE(
            "Client rejected initialization (maybe due to incompatible "
            "versions?)  client version=%" PRIu8 ".%" PRIu8 ".%" PRIu16
            ", service version=%" PRIu8 ".%" PRIu8 ".%" PRIu16,
            context->registeredClients[context->clientIndexOfServiceIndex[i]]
                ->descriptor.version.major,
            context->registeredClients[context->clientIndexOfServiceIndex[i]]
                ->descriptor.version.minor,
            context->registeredClients[context->clientIndexOfServiceIndex[i]]
                ->descriptor.version.patch,
            response->services[i].version.major,
            response->services[i].version.minor,
            response->services[i].version.patch);
      } else {
        matchedClients++;
      }
    }
  }

  CHPP_LOGI("Successfully matched %" PRIu8
            " clients with services, out of a total of %" PRIu8
            " registered clients and %" PRIu8 " discovered services",
            matchedClients, context->registeredClientCount, serviceCount);
}

/************************************************
 *  Public Functions
 ***********************************************/

bool chppDispatchDiscoveryServiceResponse(struct ChppAppState *context,
                                          const uint8_t *buf, size_t len) {
  struct ChppAppHeader *rxHeader = (struct ChppAppHeader *)buf;
  bool success = true;

  switch (rxHeader->command) {
    case CHPP_DISCOVERY_COMMAND_DISCOVER_ALL: {
      chppDiscoveryProcessDiscoverAll(context, buf, len);
      break;
    }
    default: {
      success = false;
      break;
    }
  }
  return success;
}

void chppInitiateDiscovery(struct ChppAppState *context) {
  for (uint8_t i = 0; i < CHPP_MAX_DISCOVERED_SERVICES; i++) {
    context->clientIndexOfServiceIndex[i] = CHPP_CLIENT_INDEX_NONE;
  }

  struct ChppAppHeader *request = chppMalloc(sizeof(struct ChppAppHeader));
  request->handle = CHPP_HANDLE_DISCOVERY;
  request->type = CHPP_MESSAGE_TYPE_CLIENT_REQUEST;
  request->command = CHPP_DISCOVERY_COMMAND_DISCOVER_ALL;

  chppEnqueueTxDatagramOrFail(context->transportContext, request,
                              sizeof(struct ChppAppHeader));
}
