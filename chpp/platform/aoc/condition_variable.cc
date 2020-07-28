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

#include "chpp/platform/platform_condition_variable.h"

void chppPlatformConditionVariableInit(
    struct ChppConditionVariable *condition_variable) {
  // TODO: Implement this
}

void chppPlatformConditionVariableDeinit(
    struct ChppConditionVariable *condition_variable) {
  // TODO: Implement this
}

bool chppPlatformConditionVariableWait(
    struct ChppConditionVariable *condition_variable, struct ChppMutex *mutex) {
  // TODO: Implement this
  return false;
}

void chppPlatformConditionVariableSignal(
    struct ChppConditionVariable *condition_variable) {
  // TODO: Implement this
}
