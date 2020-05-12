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

void PlatformSensorBase::initBase(usf::UsfServerHandle serverHandle,
                                  uint8_t sensorType, uint64_t minInterval,
                                  const char *sensorName) {
  mServerHandle = serverHandle;
  mSensorType = sensorType;
  mMinInterval = minInterval;

  // TODO(b/143139477): Move back to strlcpy when AOC exposes it
  strncpy(mSensorName, sensorName, kSensorNameMaxLen);
  mSensorName[kSensorNameMaxLen - 1] = '\0';
}

uint8_t PlatformSensor::getSensorType() const {
  return mSensorType;
}

uint64_t PlatformSensor::getMinInterval() const {
  return mMinInterval;
}

bool PlatformSensor::reportsBiasEvents() const {
  return PlatformSensorTypeHelpersBase::reportsBias(mSensorType);
}

bool PlatformSensor::supportsPassiveMode() const {
  return true;
}

const char *PlatformSensor::getSensorName() const {
  return mSensorName;
}

PlatformSensor::PlatformSensor(PlatformSensor &&other) {
  *this = std::move(other);
}

PlatformSensor &PlatformSensor::operator=(PlatformSensor &&other) {
  // Note: if this implementation is ever changed to depend on "this" containing
  // initialized values, the move constructor implementation must be updated.
  mServerHandle = other.mServerHandle;
  mSensorType = other.mSensorType;
  mMinInterval = other.mMinInterval;
  mSamplingId = other.mSamplingId;

  memcpy(mSensorName, other.mSensorName, kSensorNameMaxLen);

  return *this;
}

}  // namespace chre
