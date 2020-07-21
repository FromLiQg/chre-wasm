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

#ifndef CHPP_PLATFORM_SYNC_H_
#define CHPP_PLATFORM_SYNC_H_

#include <pthread.h>

#include "chpp/mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ChppNotifier {
  pthread_cond_t cond;     // Condition variable
  struct ChppMutex mutex;  // Platform-specific mutex
  uint32_t signal;
};

/**
 * Platform implementation of chppNotifierInit()
 */
void chppPlatformNotifierInit(struct ChppNotifier *notifier);

/**
 * Platform implementation of chppNotifierDeinit()
 */
void chppPlatformNotifierDeinit(struct ChppNotifier *notifier);

/**
 * Platform implementation of chppNotifierWait()
 */
uint32_t chppPlatformNotifierWait(struct ChppNotifier *notifier);

/**
 * Platform implementation of chppNotifierSignal()
 */
void chppPlatformNotifierSignal(struct ChppNotifier *notifier, uint32_t signal);

static inline void chppNotifierInit(struct ChppNotifier *notifier) {
  chppPlatformNotifierInit(notifier);
}

static inline void chppNotifierDeinit(struct ChppNotifier *notifier) {
  chppPlatformNotifierDeinit(notifier);
}

static inline uint32_t chppNotifierWait(struct ChppNotifier *notifier) {
  return chppPlatformNotifierWait(notifier);
}

static inline void chppNotifierSignal(struct ChppNotifier *notifier,
                                      uint32_t signal) {
  chppPlatformNotifierSignal(notifier, signal);
}

#ifdef __cplusplus
}
#endif

#endif  // CHPP_PLATFORM_SYNC_H_
