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

#include "chre/platform/platform_sensor.h"

#include "chre/core/sensor_type_helpers.h"

namespace chre {

void PlatformSensorBase::initBase(
    refcount::reffed_ptr<usf::UsfSensor> &usfSensor, uint8_t sensorType) {
  mUsfSensor = usfSensor;
  mSensorType = sensorType;
}

uint8_t PlatformSensor::getSensorType() const {
  return mSensorType;
}

uint64_t PlatformSensor::getMinInterval() const {
  return SensorTypeHelpers::isOneShot(mSensorType)
             ? CHRE_SENSOR_INTERVAL_DEFAULT
             : mUsfSensor->GetMinDelayNs();
  ;
}

bool PlatformSensor::reportsBiasEvents() const {
  return PlatformSensorTypeHelpersBase::reportsBias(getSensorType());
}

bool PlatformSensor::supportsPassiveMode() const {
  return true;
}

const char *PlatformSensor::getSensorName() const {
  return SensorTypeHelpers::getSensorTypeName(getSensorType());
}

PlatformSensor::PlatformSensor(PlatformSensor &&other) {
  *this = std::move(other);
}

PlatformSensor &PlatformSensor::operator=(PlatformSensor &&other) {
  // Note: if this implementation is ever changed to depend on "this" containing
  // initialized values, the move constructor implementation must be updated.
  mUsfSensor = other.mUsfSensor;
  mSensorType = other.mSensorType;

  return *this;
}

}  // namespace chre
