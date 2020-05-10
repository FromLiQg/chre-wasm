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

#ifndef CHRE_PLATFORM_USF_PLATFORM_SENSOR_MANAGER_BASE_H_
#define CHRE_PLATFORM_USF_PLATFORM_SENSOR_MANAGER_BASE_H_

#include "chre/platform/usf/usf_helper.h"
#include "chre/util/unique_ptr.h"

namespace chre {

class PlatformSensorManagerBase : public UsfHelperCallbackInterface {
 public:
  void onSensorDataEvent(uint8_t sensorType,
                         UniquePtr<uint8_t> &&eventData) override;

  bool getSensorInfo(uint8_t sensorType, SensorInfo *sensorInfo) override;

 protected:
  //! Helper used to communicate with USF.
  UsfHelper mHelper;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_USF_PLATFORM_SENSOR_MANAGER_BASE_H_
