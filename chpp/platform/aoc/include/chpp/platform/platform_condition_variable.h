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

#ifndef CHPP_PLATFORM_CONDITION_VARIABLE_H_
#define CHPP_PLATFORM_CONDITION_VARIABLE_H_

#include <stdbool.h>

#include "FreeRTOS.h"
#include "chpp/mutex.h"
#include "semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ChppConditionVariable {
  SemaphoreHandle_t semaphoreHandle;
  StaticSemaphore_t staticSemaphore;
};

/**
 * Platform implementation of chppConditionVariableInit().
 */
void chppPlatformConditionVariableInit(struct ChppConditionVariable *cv);

/**
 * Platform implementation of chppConditionVariableDeinit().
 */
void chppPlatformConditionVariableDeinit(struct ChppConditionVariable *cv);

/**
 * Platform implementation of chppConditionVariable[Timed]Wait().
 */
bool chppPlatformConditionVariableWait(struct ChppConditionVariable *cv,
                                       struct ChppMutex *mutex);
bool chppPlatformConditionVariableTimedWait(struct ChppConditionVariable *cv,
                                            struct ChppMutex *mutex,
                                            uint64_t timeoutNs);

/**
 * Platform implementation of chppConditionVariableSignal().
 */
void chppPlatformConditionVariableSignal(struct ChppConditionVariable *cv);

static inline void chppConditionVariableInit(struct ChppConditionVariable *cv) {
  chppPlatformConditionVariableInit(cv);
}

static inline void chppConditionVariableDeinit(
    struct ChppConditionVariable *cv) {
  chppPlatformConditionVariableDeinit(cv);
}

static inline bool chppConditionVariableWait(struct ChppConditionVariable *cv,
                                             struct ChppMutex *mutex) {
  return chppPlatformConditionVariableWait(cv, mutex);
}

static inline bool chppConditionVariableTimedWait(
    struct ChppConditionVariable *cv, struct ChppMutex *mutex,
    uint64_t timeoutNs) {
  return chppPlatformConditionVariableTimedWait(cv, mutex, timeoutNs);
}

static inline void chppConditionVariableSignal(
    struct ChppConditionVariable *cv) {
  chppPlatformConditionVariableSignal(cv);
}

#ifdef __cplusplus
}
#endif

#endif  // CHPP_PLATFORM_CONDITION_VARIABLE_H_
