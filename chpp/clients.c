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

#include "chpp/clients.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "chpp/app.h"
#ifdef CHPP_CLIENT_ENABLED_GNSS
#include "chpp/clients/gnss.h"
#endif
#ifdef CHPP_CLIENT_ENABLED_LOOPBACK
#include "chpp/clients/loopback.h"
#endif
#ifdef CHPP_CLIENT_ENABLED_TIMESYNC
#include "chpp/clients/timesync.h"
#endif
#ifdef CHPP_CLIENT_ENABLED_WIFI
#include "chpp/clients/wifi.h"
#endif
#ifdef CHPP_CLIENT_ENABLED_WWAN
#include "chpp/clients/wwan.h"
#endif
#include "chpp/log.h"
#include "chpp/macros.h"
#include "chpp/memory.h"
#include "chpp/time.h"
#include "chpp/transport.h"

/************************************************
 *  Prototypes
 ***********************************************/

static bool chppIsClientApiReady(struct ChppClientState *clientState);

/************************************************
 *  Private Functions
 ***********************************************/

/**
 * Determines whether a client is ready to accept commands via its API (i.e. is
 * initialized and opened). If the client is in the process of reopening, it
 * will wait for the client to reopen.
 *
 * @param clientState State of the client sending the client request.
 *
 * @return Indicates whetherthe client is ready.
 */
static bool chppIsClientApiReady(struct ChppClientState *clientState) {
  const uint64_t timeoutNs = DEFAULT_CLIENT_REQUEST_TIMEOUT_NS;
  bool result = false;

  if (clientState->initialized &&
      clientState->openState != CHPP_OPEN_STATE_CLOSED) {
    result = true;

    if (clientState->openState == CHPP_OPEN_STATE_WAITING_TO_REOPEN) {
      // Wait for service to be reopened
      chppMutexLock(&clientState->responseMutex);

      while (result && !clientState->responseReady) {
        result = chppConditionVariableTimedWait(&clientState->responseCondVar,
                                                &clientState->responseMutex,
                                                timeoutNs);
      }
      if (!clientState->responseReady) {
        CHPP_LOGE("Timeout waiting to reopen service after %" PRIu64 " ms",
                  timeoutNs / CHPP_NSEC_PER_MSEC);
        result = false;
      }

      chppMutexUnlock(&clientState->responseMutex);
    }
  }

  if (!result) {
    CHPP_LOGE("Client API not ready (initialized=%s, openState=%" PRIu8 ")",
              (clientState->initialized) ? "true" : "false",
              clientState->openState);
  }
  return result;
}

/************************************************
 *  Public Functions
 ***********************************************/

void chppRegisterCommonClients(struct ChppAppState *context) {
  UNUSED_VAR(context);
  CHPP_LOGD("Registering Clients");

#ifdef CHPP_CLIENT_ENABLED_LOOPBACK
  if (context->clientServiceSet.loopbackClient) {
    chppLoopbackClientInit(context);
  }
#endif

#ifdef CHPP_CLIENT_ENABLED_TIMESYNC
  if (context->clientServiceSet.timesyncClient) {
    chppTimesyncClientInit(context);
  }
#endif

#ifdef CHPP_CLIENT_ENABLED_WWAN
  if (context->clientServiceSet.wwanClient) {
    chppRegisterWwanClient(context);
  }
#endif

#ifdef CHPP_CLIENT_ENABLED_WIFI
  if (context->clientServiceSet.wifiClient) {
    chppRegisterWifiClient(context);
  }
#endif

#ifdef CHPP_CLIENT_ENABLED_GNSS
  if (context->clientServiceSet.gnssClient) {
    chppRegisterGnssClient(context);
  }
#endif
}

void chppDeregisterCommonClients(struct ChppAppState *context) {
  UNUSED_VAR(context);
  CHPP_LOGD("Deregistering Clients");

#ifdef CHPP_CLIENT_ENABLED_LOOPBACK
  if (context->clientServiceSet.loopbackClient) {
    chppLoopbackClientDeinit(context);
  }
#endif

#ifdef CHPP_CLIENT_ENABLED_TIMESYNC
  if (context->clientServiceSet.timesyncClient) {
    chppTimesyncClientDeinit(context);
  }
#endif

#ifdef CHPP_CLIENT_ENABLED_WWAN
  if (context->clientServiceSet.wwanClient) {
    chppDeregisterWwanClient(context);
  }
#endif

#ifdef CHPP_CLIENT_ENABLED_WIFI
  if (context->clientServiceSet.wifiClient) {
    chppDeregisterWifiClient(context);
  }
#endif

#ifdef CHPP_CLIENT_ENABLED_GNSS
  if (context->clientServiceSet.gnssClient) {
    chppDeregisterGnssClient(context);
  }
#endif
}

void chppClientInit(struct ChppClientState *clientContext, uint8_t handle) {
  clientContext->handle = handle;
  chppMutexInit(&clientContext->responseMutex);
  chppConditionVariableInit(&clientContext->responseCondVar);
  clientContext->initialized = true;
}

void chppClientDeinit(struct ChppClientState *clientContext) {
  clientContext->initialized = false;
  chppConditionVariableDeinit(&clientContext->responseCondVar);
  chppMutexDeinit(&clientContext->responseMutex);
}

void chppRegisterClient(struct ChppAppState *appContext, void *clientContext,
                        const struct ChppClient *newClient) {
  CHPP_NOT_NULL(newClient);

  if (appContext->registeredClientCount >= CHPP_MAX_REGISTERED_CLIENTS) {
    CHPP_LOGE("Cannot register new client # %" PRIu8 ". Already hit maximum",
              appContext->registeredClientCount);

  } else {
    appContext->registeredClients[appContext->registeredClientCount] =
        newClient;
    appContext->registeredClientContexts[appContext->registeredClientCount] =
        clientContext;

    char uuidText[CHPP_SERVICE_UUID_STRING_LEN];
    chppUuidToStr(newClient->descriptor.uuid, uuidText);
    CHPP_LOGI("Registered client # %" PRIu8 " with UUID=%s, version=%" PRIu8
              ".%" PRIu8 ".%" PRIu16 ", min_len=%" PRIuSIZE,
              appContext->registeredClientCount, uuidText,
              newClient->descriptor.version.major,
              newClient->descriptor.version.minor,
              newClient->descriptor.version.patch, newClient->minLength);

    appContext->registeredClientCount++;
  }
}

struct ChppAppHeader *chppAllocClientRequest(
    struct ChppClientState *clientState, size_t len) {
  CHPP_ASSERT(len >= CHPP_APP_MIN_LEN_HEADER_WITH_TRANSACTION);

  struct ChppAppHeader *result = chppMalloc(len);
  if (result != NULL) {
    result->handle = clientState->handle;
    result->type = CHPP_MESSAGE_TYPE_CLIENT_REQUEST;
    result->transaction = clientState->transaction;
    result->error = CHPP_APP_ERROR_NONE;

    clientState->transaction++;
  }
  return result;
}

struct ChppAppHeader *chppAllocClientRequestCommand(
    struct ChppClientState *clientState, uint16_t command) {
  struct ChppAppHeader *result =
      chppAllocClientRequest(clientState, sizeof(struct ChppAppHeader));

  if (result != NULL) {
    result->command = command;
  }
  return result;
}

void chppClientTimestampRequest(struct ChppRequestResponseState *rRState,
                                struct ChppAppHeader *requestHeader) {
  if (rRState->responseTimeNs == CHPP_TIME_NONE &&
      rRState->requestTimeNs != CHPP_TIME_NONE) {
    CHPP_LOGE("Sending duplicate request with transaction ID=%" PRIu8
              " while prior request with transaction ID=%" PRIu8
              " was outstanding from t=%" PRIu64,
              requestHeader->transaction, rRState->transaction,
              rRState->requestTimeNs);
  }
  rRState->requestTimeNs = chppGetCurrentTimeNs();
  rRState->responseTimeNs = CHPP_TIME_NONE;
  rRState->transaction = requestHeader->transaction;
}

bool chppClientTimestampResponse(struct ChppRequestResponseState *rRState,
                                 const struct ChppAppHeader *responseHeader) {
  uint64_t previousResponseTime = rRState->responseTimeNs;
  rRState->responseTimeNs = chppGetCurrentTimeNs();

  if (rRState->requestTimeNs == CHPP_TIME_NONE) {
    CHPP_LOGE("Received response at t=%" PRIu64
              " with no prior outstanding request",
              rRState->responseTimeNs);

  } else if (previousResponseTime != CHPP_TIME_NONE) {
    rRState->responseTimeNs = chppGetCurrentTimeNs();
    CHPP_LOGW("Received additional response at t=%" PRIu64
              " for request at t=%" PRIu64 " (RTT=%" PRIu64 ")",
              rRState->responseTimeNs, rRState->responseTimeNs,
              rRState->responseTimeNs - rRState->requestTimeNs);

  } else {
    CHPP_LOGD("Received response at t=%" PRIu64 " for request at t=%" PRIu64
              " (RTT=%" PRIu64 ")",
              rRState->responseTimeNs, rRState->responseTimeNs,
              rRState->responseTimeNs - rRState->requestTimeNs);
  }

  // TODO: Also check for timeout
  if (responseHeader->transaction != rRState->transaction) {
    CHPP_LOGE(
        "Received a response at t=%" PRIu64 " with transaction ID=%" PRIu8
        " for a different outstanding request with transaction ID=%" PRIu8,
        rRState->responseTimeNs, responseHeader->transaction,
        rRState->transaction);
    return false;
  } else {
    return true;
  }
}

bool chppSendTimestampedRequestOrFail(struct ChppClientState *clientState,
                                      struct ChppRequestResponseState *rRState,
                                      void *buf, size_t len) {
  CHPP_ASSERT(len >= CHPP_APP_MIN_LEN_HEADER_WITH_TRANSACTION);
  if (!chppIsClientApiReady(clientState)) {
    CHPP_FREE_AND_NULLIFY(buf);
    return false;
  }

  chppClientTimestampRequest(rRState, buf);
  clientState->responseReady = false;
  return chppEnqueueTxDatagramOrFail(clientState->appContext->transportContext,
                                     buf, len);
}

bool chppSendTimestampedRequestAndWait(struct ChppClientState *clientState,
                                       struct ChppRequestResponseState *rRState,
                                       void *buf, size_t len) {
  return chppSendTimestampedRequestAndWaitTimeout(
      clientState, rRState, buf, len, DEFAULT_CLIENT_REQUEST_TIMEOUT_NS);
}

bool chppSendTimestampedRequestAndWaitTimeout(
    struct ChppClientState *clientState,
    struct ChppRequestResponseState *rRState, void *buf, size_t len,
    uint64_t timeoutNs) {
  bool result =
      chppSendTimestampedRequestOrFail(clientState, rRState, buf, len);

  if (result) {
    chppMutexLock(&clientState->responseMutex);

    while (result && !clientState->responseReady) {
      result = chppConditionVariableTimedWait(&clientState->responseCondVar,
                                              &clientState->responseMutex,
                                              timeoutNs);
    }
    if (!clientState->responseReady) {
      CHPP_LOGE("Timeout waiting on service response after %" PRIu64 " ms",
                timeoutNs / CHPP_NSEC_PER_MSEC);
      result = false;
    }

    chppMutexUnlock(&clientState->responseMutex);
  }

  return result;
}

void chppClientPseudoOpen(struct ChppClientState *clientState) {
  if (clientState->openState == CHPP_OPEN_STATE_CLOSED) {
    clientState->openState = CHPP_OPEN_STATE_PSEUDO_OPEN;
  }
}

bool chppClientSendOpenRequest(struct ChppClientState *clientState,
                               struct ChppRequestResponseState *openRRState,
                               uint16_t openCommand, bool reopen) {
  bool result = false;

  struct ChppAppHeader *request =
      chppAllocClientRequestCommand(clientState, openCommand);

  if (request == NULL) {
    CHPP_LOG_OOM();

  } else if (reopen) {
    CHPP_LOGW("Reopening service after reset");
    clientState->openState = CHPP_OPEN_STATE_REOPENING;
    if (!chppSendTimestampedRequestOrFail(clientState, openRRState, request,
                                          sizeof(*request))) {
      clientState->openState = CHPP_OPEN_STATE_CLOSED;
      CHPP_LOGE("Failed to reopen service");
      CHPP_PROD_ASSERT(false);
    } else {
      clientState->openState = CHPP_OPEN_STATE_WAITING_TO_REOPEN;
      result = true;
    }

  } else {
    CHPP_LOGI("Opening service");
    clientState->openState = CHPP_OPEN_STATE_OPENING;
    if (!chppSendTimestampedRequestAndWait(clientState, openRRState, request,
                                           sizeof(*request))) {
      clientState->openState = CHPP_OPEN_STATE_CLOSED;
      CHPP_LOGE("Failed to open service");
    }
    result = (clientState->openState == CHPP_OPEN_STATE_OPENED);
  }

  return result;
}

void chppClientProcessOpenResponse(struct ChppClientState *clientState,
                                   uint8_t *buf, size_t len) {
  UNUSED_VAR(len);  // Necessary depending on assert macro below
  // Assert condition already guaranteed by chppAppProcessRxDatagram() but
  // checking again since this is a public function
  CHPP_ASSERT(len >= sizeof(struct ChppAppHeader));

  struct ChppAppHeader *rxHeader = (struct ChppAppHeader *)buf;
  if (rxHeader->error != CHPP_APP_ERROR_NONE) {
    CHPP_LOGE("Service %sopen failed at service",
              (clientState->openState == CHPP_OPEN_STATE_WAITING_TO_REOPEN)
                  ? "re"
                  : "");
    clientState->openState = CHPP_OPEN_STATE_CLOSED;
  } else {
    CHPP_LOGI("Service %sopen succeeded at service",
              (clientState->openState == CHPP_OPEN_STATE_WAITING_TO_REOPEN)
                  ? "re"
                  : "");
    clientState->openState = CHPP_OPEN_STATE_OPENED;
  }
}
