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

#ifndef CHRE_PLATFORM_USF_PLATFORM_SENSOR_BASE_H_
#define CHRE_PLATFORM_USF_PLATFORM_SENSOR_BASE_H_

#include "chre/util/optional.h"

#include "usf/usf.h"
#include "usf/usf_sensor.h"

namespace chre {

//! The max length of sensorName
constexpr size_t kSensorNameMaxLen = 64;

class PlatformSensorBase {
 public:
  /**
   * Initializes various members of PlatformSensorBase.
   */
  void initBase(refcount::reffed_ptr<usf::UsfSensor> &usfSensor,
                uint8_t sensorType);

  usf::UsfServerHandle getServerHandle() const {
    return mUsfSensor->GetHandle();
  }

  bool isSamplingIdValid() const {
    return mSamplingId.has_value();
  }

  void setSamplingIdInvalid() {
    mSamplingId.reset();
  }

  void setSamplingId(uint32_t samplingId) {
    mSamplingId = samplingId;
  }

  uint32_t getSamplingId() const {
    return mSamplingId.value();
  }

  const refcount::reffed_ptr<usf::UsfSensor> &getUsfSensor() const {
    return mUsfSensor;
  }

 protected:
  //! The USF sensor instance for this sensor.
  refcount::reffed_ptr<usf::UsfSensor> mUsfSensor;

  //! The sampling ID of the active request with this sensor.
  Optional<uint32_t> mSamplingId;

  //! Sensor type of this sensor. Needed because USF shares the same sensor
  //! type for two different CHRE sensors (ACCELEROMETER_TEMPERATURE and
  //! GYROSCOPE_TEMPERATURE).
  uint8_t mSensorType;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_USF_PLATFORM_SENSOR_BASE_H_
