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

#include "chpp/services/wwan.h"
#include "chre/pal/wwan.h"

/************************************************
 *  Prototypes
 ***********************************************/

void chppDispatchWwan(void *serviceContext, uint8_t *buf, size_t len);

/************************************************
 *  Private Definitions
 ***********************************************/

/**
 * Configuration parameters for this service
 */
static const struct ChppService wwanServiceConfig = {
    .descriptor.uuid = {0x0d, 0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f, 0x0d,
                        0x0e, 0x0a, 0x0d, 0x0b, 0x0e, 0x0e, 0x0f},  // TODO

    // Human-readable name
    .descriptor.name = "WWAN",

    .descriptor.versionMajor = 1,

    .descriptor.versionMinor = 0,

    .descriptor.versionPatch = 0,

    // Dispatch function pointer
    .dispatchFunctionPtr = &chppDispatchWwan,

    // Min length is the entire header
    .minLength = sizeof(struct ChppAppHeader),
};

/**
 * Data structure used by the Get Capabilities Response.
 */
CHPP_PACKED_START
struct ChppWwanGetCapabilitiesResponse {
  struct ChppServiceBasicResponse common;
  uint32_t capabilities;
} CHPP_PACKED_ATTR;
CHPP_PACKED_END

/**
 * Structure to maintain state for the WWAN service and its Request/Response
 * (RR) functionality.
 */
struct ChppWwanServiceState {
  struct ChppServiceState service;             // WWAN service state
  const struct chrePalWwanApi *api;            // WWAN PAL API
  struct ChppServiceRRState getCapabilities;   // Get Capabilities state
  struct ChppServiceRRState getCellInfoAsync;  // Get Cell Info Async state
};

// Note: This global definition of gWwanServiceContext supports only one
// instance of the CHPP WWAN service at a time. This limitation is primarily due
// to the PAL API.
// It would be possible to generate different API and callback pointers to
// support multiple instances of the service or modify the PAL API to pass a
// void* for context, but this is not necessary in the current version of CHPP.
// In such case, wwanServiceContext would be allocated dynamically as part of
// chppWwanServiceInit(), e.g.
//   struct ChppWwanServiceState *wwanServiceContext = chppMalloc(...);
// instead of globally here.
struct ChppWwanServiceState gWwanServiceContext;

/**
 * Commands used by the WWAN (cellular) Service
 */
enum ChppWwanCommands {
  // Retrieves a set of flags indicating supported features.
  CHPP_WWAN_GET_CAPABILITIES = 0x2010,

  // Query information about the current serving cell and its neighbors.
  CHPP_WWAN_GET_CELLINFO_ASYNC = 0x2020,
};

/************************************************
 *  Prototypes
 ***********************************************/

void chppWwanGetCapabilities(struct ChppWwanServiceState *wwanServiceContext,
                             struct ChppAppHeader *requestHeader);
void chppWwanGetCellInfoAsync(struct ChppWwanServiceState *wwanServiceContext,
                              struct ChppAppHeader *requestHeader);

/************************************************
 *  Private Functions
 ***********************************************/

/**
 * Dispatches an Rx Datagram from the transport layer that is determined to be
 * for the WWAN service (e.g. a WWAN request from a client).
 *
 * This function is called from the app layer using its function pointer given
 * during service registration.
 *
 * @param serviceContext Maintains status for each service instance.
 * @param buf Input data. Cannot be null.
 * @param len Length of input data in bytes.
 */
void chppDispatchWwan(void *serviceContext, uint8_t *buf, size_t len) {
  struct ChppAppHeader *rxHeader = (struct ChppAppHeader *)buf;
  struct ChppWwanServiceState *wwanServiceContext =
      (struct ChppWwanServiceState *)serviceContext;

  UNUSED_VAR(len);

  switch (rxHeader->type) {  // TODO: separate request / notification callbacks
    case CHPP_MESSAGE_TYPE_CLIENT_REQUEST: {
      switch (rxHeader->command) {
        case CHPP_WWAN_GET_CAPABILITIES: {
          chppTimestampRequest(&wwanServiceContext->getCapabilities, rxHeader);
          chppWwanGetCapabilities(wwanServiceContext, rxHeader);
          break;
        }

        case CHPP_WWAN_GET_CELLINFO_ASYNC: {
          chppTimestampRequest(&wwanServiceContext->getCellInfoAsync, rxHeader);
          chppWwanGetCellInfoAsync(wwanServiceContext, rxHeader);
          break;
        }

        default: {  // TODO: handle in common code
          LOGE(
              "Received unknown WWAN client request: command = %#x, "
              "transaction ID = %" PRIu8,
              rxHeader->command, rxHeader->transaction);

          // Allocate the response
          struct ChppServiceBasicResponse *response =
              chppAllocServiceResponseFixed(rxHeader,
                                            struct ChppServiceBasicResponse);

          // Populate the response
          response->error = CHPP_SERVICE_ERROR_INVALID_COMMAND;

          // Send out response datagram
          chppEnqueueTxDatagramOrFail(
              wwanServiceContext->service.appContext->transportContext,
              response, sizeof(*response));

          break;
        }
      }
      break;
    }

    case CHPP_MESSAGE_TYPE_CLIENT_NOTIFICATION: {
      LOGE(
          "Received unknown WWAN client notification: command = %#x, "
          "transaction ID = %" PRIu8,
          rxHeader->command, rxHeader->transaction);
      break;
    }

    default: {  // TODO: handle in common code
      LOGE(
          "WWAN service received unknown message type = %#x, "
          "command = %#x, transaction = %d",
          rxHeader->type, rxHeader->command, rxHeader->transaction);
    }
  }
}
/**
 * Retrieves a set of flags indicating the WWAN features supported by the
 * current implementation.
 *
 * @param serviceContext Maintains status for each service instance.
 * @param requestHeader App layer header of the request.
 */
void chppWwanGetCapabilities(struct ChppWwanServiceState *wwanServiceContext,
                             struct ChppAppHeader *requestHeader) {
  // Allocate the response
  struct ChppWwanGetCapabilitiesResponse *response =
      chppAllocServiceResponseFixed(requestHeader,
                                    struct ChppWwanGetCapabilitiesResponse);

  // Populate the response
  response->capabilities = gWwanServiceContext.api->getCapabilities();
  response->common.error = CHPP_SERVICE_ERROR_NONE;

  // Timestamp and send out response datagram
  chppSendTimestampedResponseOrFail(&wwanServiceContext->service,
                                    &wwanServiceContext->getCapabilities,
                                    response, sizeof(*response));
}

/**
 * Query information about the current serving cell and its neighbors in
 * response to a client request. This does not perform a network scan, but
 * should return state from the current network registration data stored in the
 * cellular modem.
 *
 * This function returns an error code synchronously. The requested cellular
 * information shall be returned asynchronously to the client via the
 * chppPlatformWwanCellInfoResultEvent() server response.
 *
 * @param serviceContext Maintains status for each service instance.
 * @param requestHeader App layer header of the request.
 */
void chppWwanGetCellInfoAsync(struct ChppWwanServiceState *wwanServiceContext,
                              struct ChppAppHeader *requestHeader) {
  // Register for callback
  if (!gWwanServiceContext.api->requestCellInfo()) {
    // Error occurred, send a synchronous error response
    struct ChppServiceBasicResponse *response = chppAllocServiceResponseFixed(
        requestHeader, struct ChppServiceBasicResponse);
    response->error = CHPP_SERVICE_ERROR_UNSPECIFIED;
    chppSendTimestampedResponseOrFail(&wwanServiceContext->service,
                                      &wwanServiceContext->getCellInfoAsync,
                                      response, sizeof(*response));
  }
}

void chppWwanCellInfoResultCallback(struct chreWwanCellInfoResult *result) {
  // Recover state
  struct ChppServiceRRState *rRState =
      (struct ChppServiceRRState *)result->cookie;
  struct ChppWwanServiceState *wwanServiceContext =
      container_of(rRState, struct ChppWwanServiceState, getCellInfoAsync);

  // Recreate request header
  struct ChppAppHeader requestHeader = {
      .handle = wwanServiceContext->service.handle,
      .transaction = rRState->transaction,
      .command = CHPP_WWAN_GET_CELLINFO_ASYNC,
  };

  // Craft response per parser script
  // TODO, this is a placeholder
  size_t responseLen = sizeof(struct ChppServiceBasicResponse) + 0 + 0;
  struct ChppServiceBasicResponse *response = chppAllocServiceResponseFixed(
      &requestHeader, struct ChppServiceBasicResponse);

  if (response == NULL) {
    LOG_OOM("WwanGetCellInfoResponseAsync response of %zu bytes", responseLen);
    CHPP_ASSERT(false);
  }

  // Populate remaining parts of response per parser script
  // TODO, this is a placeholder
  response->error = CHPP_SERVICE_ERROR_NONE;

  // Timestamp and send out response datagram
  chppSendTimestampedResponseOrFail(&wwanServiceContext->service, rRState,
                                    response, responseLen);
}

/************************************************
 *  Public Functions
 ***********************************************/

void chppWwanServiceInit(struct ChppAppState *appContext) {
  static const struct chrePalWwanCallbacks palCallbacks = {
      .cellInfoResultCallback = chppWwanCellInfoResultCallback,
  };

  // Initialize platform
  gWwanServiceContext.api = chrePalWwanGetApi(CHRE_PAL_WWAN_API_V1_4);
  if (gWwanServiceContext.api == NULL) {
    LOGE(
        "WWAN PAL API version not compatible with CHPP. Cannot register WWAN "
        "service");
    CHPP_DEBUG_ASSERT(false);
  } else if (!gWwanServiceContext.api->open(appContext->systemApi,
                                            &palCallbacks)) {
    // initialization failed, don't register service
    LOGE("WWAN PAL API initialization failed. Cannot register WWAN service");
    CHPP_DEBUG_ASSERT(false);
  } else {
    // Register service
    gWwanServiceContext.service.appContext = appContext;
    gWwanServiceContext.service.handle = chppRegisterService(
        appContext, (void *)&gWwanServiceContext, &wwanServiceConfig);
    CHPP_DEBUG_ASSERT(gWwanServiceContext.service.handle);
  }
}
