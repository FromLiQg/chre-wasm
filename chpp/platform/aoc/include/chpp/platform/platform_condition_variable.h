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

#include "chpp/mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ChppConditionVariable {
  uint8_t bufferVariable;  // temporary element to compile
};

/**
 * Platform implementation of chppConditionVariableInit().
 */
void chppPlatformConditionVariableInit(
    struct ChppConditionVariable *condition_variable);

/**
 * Platform implementation of chppConditionVariableDeinit().
 */
void chppPlatformConditionVariableDeinit(
    struct ChppConditionVariable *condition_variable);

/**
 * Platform implementation of chppConditionVariableWait().
 */
bool chppPlatformConditionVariableWait(
    struct ChppConditionVariable *condition_variable, struct ChppMutex *mutex);

/**
 * Platform implementation of chppConditionVariableSignal().
 */
void chppPlatformConditionVariableSignal(
    struct ChppConditionVariable *condition_variable);

static inline void chppConditionVariableInit(
    struct ChppConditionVariable *condition_variable) {
  chppPlatformConditionVariableInit(condition_variable);
}

static inline void chppConditionVariableDeinit(
    struct ChppConditionVariable *condition_variable) {
  chppPlatformConditionVariableDeinit(condition_variable);
}

static inline bool chppConditionVariableWait(
    struct ChppConditionVariable *condition_variable, struct ChppMutex *mutex) {
  return chppPlatformConditionVariableWait(condition_variable, mutex);
}

static inline void chppConditionVariableSignal(
    struct ChppConditionVariable *condition_variable) {
  chppPlatformConditionVariableSignal(condition_variable);
}

#ifdef __cplusplus
}
#endif

#endif  // CHPP_PLATFORM_CONDITION_VARIABLE_H_
