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
#include "chre/platform/platform_sensor_type_helpers.h"
#include "chre/target_platform/assert.h"
#include "chre/util/macros.h"

#ifdef CHREX_SENSOR_SUPPORT
#include "chre/extensions/platform/vendor_sensor_types.h"
#endif  // CHREX_SENSOR_SUPPORT

namespace chre {

ReportingMode PlatformSensorTypeHelpers::getVendorSensorReportingMode(
    uint8_t /* sensorType */) {
  // TODO: Stubbed out, implement this
  return ReportingMode::Continuous;
}

bool PlatformSensorTypeHelpers::getVendorSensorIsCalibrated(
    uint8_t /* sensorType */) {
  // TODO: Stubbed out, implement this
  return false;
}

bool PlatformSensorTypeHelpers::getVendorSensorBiasEventType(
    uint8_t /* sensorType */, uint16_t * /* eventType */) {
  // TODO: Stubbed out, implement this
  return false;
}

const char *PlatformSensorTypeHelpers::getVendorSensorTypeName(
    uint8_t sensorType) {
#ifdef CHREX_SENSOR_SUPPORT
  return extension::vendorSensorTypeName(sensorType);
#else
  UNUSED_VAR(sensorType);
  return "";
#endif
}

size_t PlatformSensorTypeHelpers::getVendorSensorLastEventSize(
    uint8_t /* sensorType */) {
  // TODO: Stubbed out, implement this
  return 0;
}

void PlatformSensorTypeHelpers::getVendorLastSample(
    uint8_t /* sensorType */, const ChreSensorData * /* event */,
    ChreSensorData * /* lastEvent */) {
  // TODO: Stubbed out, implement this
}

bool PlatformSensorTypeHelpersBase::reportsBias(uint8_t sensorType) {
  switch (sensorType) {
    case CHRE_SENSOR_TYPE_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_GYROSCOPE:
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD:
      return true;
    default:
      return false;
  }
}

SensorSampleType
PlatformSensorTypeHelpersBase::getSensorSampleTypeFromSensorType(
    uint8_t sensorType) {
  switch (sensorType) {
    case CHRE_SENSOR_TYPE_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_GYROSCOPE:
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD:
      return SensorSampleType::ThreeAxis;
    case CHRE_SENSOR_TYPE_PRESSURE:
    case CHRE_SENSOR_TYPE_LIGHT:
    case CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE:
    case CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE:
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD_TEMPERATURE:
      return SensorSampleType::Float;
    case CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT:
    case CHRE_SENSOR_TYPE_STATIONARY_DETECT:
    case CHRE_SENSOR_TYPE_STEP_DETECT:
      return SensorSampleType::Occurrence;
    case CHRE_SENSOR_TYPE_PROXIMITY:
      return SensorSampleType::Byte;
    case CHRE_SENSOR_TYPE_STEP_COUNTER:
      return SensorSampleType::Uint64;
    default:
#ifdef CHREX_SENSOR_SUPPORT
      return extension::vendorSensorSampleTypeFromSensorType(sensorType);
#else
      // Update implementation to prevent undefined from being used.
      CHRE_ASSERT(false);
      return SensorSampleType::Unknown;
#endif
  }
}

usf::UsfSensorReportingMode PlatformSensorTypeHelpersBase::getUsfReportingMode(
    ReportingMode mode) {
  if (mode == ReportingMode::OnChange) {
    return usf::kUsfSensorReportOnChange;
  } else if (mode == ReportingMode::OneShot) {
    return usf::kUsfSensorReportOneShot;
  } else {
    return usf::kUsfSensorReportContinuous;
  }
}

bool PlatformSensorTypeHelpersBase::convertUsfToChreSensorType(
    usf::UsfSensorType usfSensorType, uint8_t *chreSensorType) {
  bool success = true;
  switch (usfSensorType) {
    case usf::UsfSensorType::kUsfSensorAccelerometer:
      *chreSensorType = CHRE_SENSOR_TYPE_ACCELEROMETER;
      break;
    case usf::UsfSensorType::kUsfSensorGyroscope:
      *chreSensorType = CHRE_SENSOR_TYPE_GYROSCOPE;
      break;
    case usf::UsfSensorType::kUsfSensorProx:
      *chreSensorType = CHRE_SENSOR_TYPE_PROXIMITY;
      break;
    case usf::UsfSensorType::kUsfSensorBaro:
      *chreSensorType = CHRE_SENSOR_TYPE_PRESSURE;
      break;
    case usf::UsfSensorType::kUsfSensorMag:
      *chreSensorType = CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD;
      break;
    case usf::UsfSensorType::kUsfSensorMagTemp:
      *chreSensorType = CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD_TEMPERATURE;
      break;
    case usf::UsfSensorType::kUsfSensorImuTemp:
      *chreSensorType = CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE;
      break;
    case usf::UsfSensorType::kUsfSensorAmbientLight:
      *chreSensorType = CHRE_SENSOR_TYPE_LIGHT;
      break;
    case usf::UsfSensorType::kUsfSensorMotionDetect:
      *chreSensorType = CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT;
      break;
    case usf::UsfSensorType::kUsfSensorStationaryDetect:
      *chreSensorType = CHRE_SENSOR_TYPE_STATIONARY_DETECT;
      break;
    case usf::UsfSensorType::kUsfSensorStepDetector:
      *chreSensorType = CHRE_SENSOR_TYPE_STEP_DETECT;
      break;
    case usf::UsfSensorType::kUsfSensorStepCounter:
      *chreSensorType = CHRE_SENSOR_TYPE_STEP_COUNTER;
      break;
    default:
#ifdef CHREX_SENSOR_SUPPORT
      success = extension::vendorConvertUsfToChreSensorType(usfSensorType,
                                                            chreSensorType);
#else
      // Don't print anything as USF exposes sensor types CHRE doesn't care
      // about (e.g. Camera Vsync, and color)
      success = false;
#endif
      break;
  }
  return success;
}

uint8_t PlatformSensorTypeHelpersBase::convertUsfToChreSampleAccuracy(
    usf::UsfSampleAccuracy usfSampleAccuracy) {
  switch (usfSampleAccuracy) {
    case usf::kUsfSampleAccuracyUnreliable:
      return CHRE_SENSOR_ACCURACY_UNRELIABLE;
    case usf::kUsfSampleAccuracyLow:
      return CHRE_SENSOR_ACCURACY_LOW;
    case usf::kUsfSampleAccuracyMedium:
      return CHRE_SENSOR_ACCURACY_MEDIUM;
    case usf::kUsfSampleAccuracyHigh:
      return CHRE_SENSOR_ACCURACY_HIGH;
    case usf::kUsfSampleAccuracyUnknown:
    default:
      return CHRE_SENSOR_ACCURACY_UNKNOWN;
  }
}

}  // namespace chre
