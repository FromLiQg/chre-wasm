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

#ifndef CHPP_CLIENT_TIMESYNC_H_
#define CHPP_CLIENT_TIMESYNC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "chpp/app.h"
#include "chpp/clients.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Default number of measurements for timesync
 */
#ifndef CHPP_CLIENT_TIMESYNC_DEFAULT_MEASUREMENT_COUNT
#define CHPP_CLIENT_TIMESYNC_DEFAULT_MEASUREMENT_COUNT 5
#endif

/**
 * Timesync Results.
 */
struct ChppTimesyncResult {
  enum ChppAppErrorCode error;  // Indicates success or error type
  int64_t offsetNs;             // Time offset between client and service
  uint64_t rttNs;               // RTT
};

/************************************************
 *  Public functions
 ***********************************************/

/**
 * Initializes the client.
 *
 * @param context Maintains status for each app layer instance.
 */
void chppTimesyncClientInit(struct ChppAppState *context);

/**
 * Deinitializes the client.
 *
 * @param context Maintains status for each app layer instance.
 */
void chppTimesyncClientDeinit(struct ChppAppState *context);

/**
 * Dispatches an Rx Datagram from the transport layer that is determined to
 * be for the CHPP Timesync Client.
 *
 * @param context Maintains status for each app layer instance.
 * @param buf Input (response) datagram. Cannot be null.
 * @param len Length of input data in bytes.
 */
bool chppDispatchTimesyncServiceResponse(struct ChppAppState *context,
                                         const uint8_t *buf, size_t len);

/**
 * Initiates a CHPP timesync to measure time offset of the service.
 * Note that only one GetTimesync may be run at any time.
 *
 * @param context Maintains status for each app layer instance.
 */
struct ChppTimesyncResult chppGetTimesync(struct ChppAppState *context);

#ifdef __cplusplus
}
#endif

#endif  // CHPP_CLIENT_TIMESYNC_H_
