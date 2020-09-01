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

#ifndef CHPP_PLATFORM_INIT_H_
#define CHPP_PLATFORM_INIT_H_

#include "chpp/app.h"

namespace chpp {

/**
 * Initializes all CHPP instance and threads.
 */
void init();

/**
 * Deinitializes all CHPP instance and threads.
 */
void deinit();

/**
 * @return The ChppAppState associated with the provided link type.
 */
ChppAppState *getChppAppState(ChppLinkType linkType);

}  // namespace chpp

#endif  // CHPP_PLATFORM_INIT_H_
