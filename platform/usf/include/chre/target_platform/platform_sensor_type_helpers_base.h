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

#ifndef CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_
#define CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_

#include "usf/usf_sensor_defs.h"

namespace chre {

/**
 * This SensorSampleType is designed to help classify sensor's data type in
 * event handling.
 * TODO: This is a placeholder for AOC and not fully implemented
 */
enum class SensorSampleType {
  Byte,
  Float,
  Occurrence,
  ThreeAxis,
  Unknown,
};

/**
 * Can be used to expose static methods to the PlatformSensorTypeHelpers class
 * for use in working with vendor sensor types. Currently, this is stubbed out
 * in the AOC implementation.
 */
class PlatformSensorTypeHelpersBase {
 public:
  /**
   * Maps a sensorType to its SensorSampleType.
   *
   * @param sensorType The type of the sensor to obtain its SensorSampleType
   *     for.
   * @return The SensorSampleType of the sensorType.
   */
  static SensorSampleType getSensorSampleTypeFromSensorType(uint8_t sensorType);

  /**
   * @return Whether the given sensor type reports bias events.
   */
  static bool reportsBias(uint8_t sensorType);

  //! Helper functions used to convert between USF and CHRE types
  static usf::UsfSensorReportingMode getUsfReportingMode(ReportingMode mode);
  static bool convertUsfToChreSensorType(usf::UsfSensorType usfSensorType,
                                         uint8_t *chreSensorType);
  static uint8_t convertUsfToChreSampleAccuracy(
      usf::UsfSampleAccuracy usfSampleAccuracy);
};

}  // namespace chre

#endif  // CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_
