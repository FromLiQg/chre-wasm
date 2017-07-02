/*
 * Copyright (C) 2017 The Android Open Source Project
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
#include "nano_calibration.h"

#include <cmath>
#include <cstring>

#ifdef DIVERSITY_CHECK_ENABLED
#include "calibration/common/diversity_checker.h"
#endif  // DIVERSITY_CHECK_ENABLED

#include "calibration/util/cal_log.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"

namespace nano_calibration {

namespace {
#ifdef NANO_SENSOR_CAL_DBG_ENABLED
#define NANO_CAL_LOG(tag, format, ...) CAL_DEBUG_LOG(tag, format, ##__VA_ARGS__)
#else
#define NANO_CAL_LOG(tag, format, ...) chreLogNull(format, ##__VA_ARGS__)
#endif

#ifdef SPHERE_FIT_ENABLED
constexpr size_t kSamplesToAverageForOdrEstimateMag = 10;

// Unit conversion: nanoseconds to seconds.
constexpr float kNanosToSec = 1.0e-9f;


// Helper function that estimates the ODR based on the incoming data timestamp.
void SamplingRateEstimate(struct SampleRateData *sample_rate_data,
                          float *mean_sampling_rate_hz,
                          uint64_t timestamp_nanos,
                          bool reset_stats) {
  // If 'mean_sampling_rate_hz' is not nullptr then this function just reads
  // out the estimate of the sampling rate.
  if (mean_sampling_rate_hz != nullptr) {
    if (sample_rate_data->num_samples > 1 &&
        sample_rate_data->time_delta_accumulator > 0) {
      // Computes the final mean calculation.
      *mean_sampling_rate_hz =
          sample_rate_data->num_samples /
          (static_cast<float>(sample_rate_data->time_delta_accumulator) *
           kNanosToSec);
    } else {
      // Not enough samples to compute a valid sample rate estimate. Indicate
      // this with a -1 value.
      *mean_sampling_rate_hz = -1.0f;
    }
    reset_stats = true;
  }

  // Resets the sampling rate mean estimator data.
  if (reset_stats) {
    sample_rate_data->last_timestamp_nanos = 0;
    sample_rate_data->time_delta_accumulator = 0;
    sample_rate_data->num_samples = 0;
    return;
  }

  // Skip adding this data to the accumulator if:
  //   1. A bad timestamp was received (i.e., time not monotonic).
  //   2. 'last_timestamp_nanos' is zero.
  if (timestamp_nanos <= sample_rate_data->last_timestamp_nanos ||
      sample_rate_data->last_timestamp_nanos == 0) {
    sample_rate_data->last_timestamp_nanos = timestamp_nanos;
    return;
  }

  // Increments the number of samples.
  sample_rate_data->num_samples++;

  // Accumulate the time steps.
  sample_rate_data->time_delta_accumulator += timestamp_nanos -
      sample_rate_data->last_timestamp_nanos;
  sample_rate_data->last_timestamp_nanos = timestamp_nanos;
}
#endif  // SPHERE_FIT_ENABLED

// Helper function that resets calibration data to a known initial state.
void ResetCalParams(struct ashCalParams *cal_params) {
  // Puts 'cal_params' into a known "default" pass-through state (i.e.,
  // calibration data will not influence sensor streams).
  memset(cal_params, 0, sizeof(struct ashCalParams));

  // Sets 'scaleFactor' to unity.
  cal_params->scaleFactor[0] = 1.0f;
  cal_params->scaleFactor[1] = 1.0f;
  cal_params->scaleFactor[2] = 1.0f;
}

// Helper function that resets calibration info to a known initial state.
void ResetCalInfo(struct ashCalInfo *cal_info) {
  // Puts 'cal_info' into a known "default" pass-through state (i.e.,
  // calibration info will not influence sensor streams).
  memset(cal_info, 0, sizeof(struct ashCalInfo));

  // Sets 'compMatrix' to the Identity matrix.
  cal_info->compMatrix[0] = 1.0f;
  cal_info->compMatrix[4] = 1.0f;
  cal_info->compMatrix[8] = 1.0f;

  cal_info->accuracy = ASH_CAL_ACCURACY_MEDIUM;
}

// Helper function to print out calibration data.
void PrintAshCalParams(const struct ashCalParams &cal_params, const char *tag) {
  NANO_CAL_LOG(tag, "Offset | Temp [Celsius]: %.6f, %.6f, %.6f | %.6f",
               cal_params.offset[0], cal_params.offset[1], cal_params.offset[2],
               cal_params.offsetTempCelsius);

  NANO_CAL_LOG(
      tag, "Temp Sensitivity | Intercept: %.6f, %.6f, %.6f | %.6f, %.6f, %.6f",
      cal_params.tempSensitivity[0], cal_params.tempSensitivity[1],
      cal_params.tempSensitivity[2], cal_params.tempIntercept[0],
      cal_params.tempIntercept[1], cal_params.tempIntercept[2]);

  NANO_CAL_LOG(tag, "Scale Factor: %.6f, %.6f, %.6f", cal_params.scaleFactor[0],
               cal_params.scaleFactor[1], cal_params.scaleFactor[2]);

  NANO_CAL_LOG(tag, "Cross-Axis in [yx, zx, zy] order: %.6f, %.6f, %.6f",
               cal_params.crossAxis[0], cal_params.crossAxis[1],
               cal_params.crossAxis[2]);
}

// Detects and converts Factory Calibration data into a format consumable by the
// runtime accelerometer calibration algorithm.
#ifdef ACCEL_CAL_ENABLED
void HandleAccelFactoryCalibration(struct ashCalParams *cal_params) {
  // Checks for factory calibration data and performs any processing on the
  // input to make it compatible with this runtime algorithm. NOTE: Factory
  // calibrations are distinguished by 'offsetSource'=ASH_CAL_PARAMS_SOURCE_NONE
  // and 'offsetTempCelsiusSource'=ASH_CAL_PARAMS_SOURCE_FACTORY.
  bool factory_cal_detected =
      cal_params->offsetSource == ASH_CAL_PARAMS_SOURCE_NONE &&
      cal_params->offsetTempCelsiusSource == ASH_CAL_PARAMS_SOURCE_FACTORY;

  if (factory_cal_detected) {
    // Prints the received factory data.
    PrintAshCalParams(*cal_params,"[NanoSensorCal:ACCEL_FACTORY_CAL]");

    // Sets the parameter source to runtime calibration.
    cal_params->offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    cal_params->offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;

    // Ensures that the offset vector is zero in case it has been overwritten by
    // mistake.
    memset(cal_params->offset, 0, sizeof(cal_params->offset));

    //TODO: Incorporate over-temperature offset compensation.
  }
}
#endif  // ACCEL_CAL_ENABLED

// Detects and converts Factory Calibration data into a format consumable by the
// runtime gyroscope calibration algorithm.
#ifdef GYRO_CAL_ENABLED
void HandleGyroFactoryCalibration(struct ashCalParams *cal_params) {
#ifdef OVERTEMPCAL_GYRO_ENABLED
  // Checks for factory calibration data and performs any processing on the
  // input to make it compatible with this runtime algorithm. NOTE: Factory
  // calibrations are distinguished by 'offsetSource'=ASH_CAL_PARAMS_SOURCE_NONE
  // and 'offsetTempCelsiusSource'=ASH_CAL_PARAMS_SOURCE_FACTORY
  bool factory_cal_detected =
      cal_params->offsetSource == ASH_CAL_PARAMS_SOURCE_NONE &&
      cal_params->offsetTempCelsiusSource == ASH_CAL_PARAMS_SOURCE_FACTORY &&
      cal_params->tempSensitivitySource == ASH_CAL_PARAMS_SOURCE_FACTORY &&
      cal_params->tempInterceptSource == ASH_CAL_PARAMS_SOURCE_FACTORY;

  if (factory_cal_detected) {
    // Prints the received factory data.
    PrintAshCalParams(*cal_params,"[NanoSensorCal:GYRO_FACTORY_CAL]");

    // Sets the parameter source to runtime calibration.
    cal_params->offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    cal_params->offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    cal_params->tempSensitivitySource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    cal_params->tempInterceptSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;

    // Since the Factory-Cal OTC model is computed from raw measured data and
    // the 'offset' at 'offsetTempCelsius' is removed from the input sensor
    // stream, the intercept must be adjusted so that the runtime OTC produces a
    // zero offset vector at 'offsetTempCelsius'.
    for (size_t i = 0; i < 3; i++) {
      // Shifts the OTC linear model intercept by 'offset_at_offsetTempCelsius'.
      float offset_at_offsetTempCelsius =
          cal_params->tempSensitivity[i] * cal_params->offsetTempCelsius +
          cal_params->tempIntercept[i];
      cal_params->tempIntercept[i] -= offset_at_offsetTempCelsius;
    }

    // Ensures that the offset vector is zero in case it has been overwritten by
    // mistake.
    memset(cal_params->offset, 0, sizeof(cal_params->offset));
  }
#else
  // Checks for factory calibration data and performs any processing on the
  // input to make it compatible with this runtime algorithm.
  bool factory_cal_detected =
      cal_params->offsetSource == ASH_CAL_PARAMS_SOURCE_NONE &&
      cal_params->offsetTempCelsiusSource == ASH_CAL_PARAMS_SOURCE_FACTORY;

  if (factory_cal_detected) {
    // Prints the received factory data.
    PrintAshCalParams(*cal_params,"[NanoSensorCal:GYRO_FACTORY_CAL]");

    // Sets the parameter source to runtime calibration.
    cal_params->offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    cal_params->offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;

    // Ensures that the offset vector is zero in case it has been overwritten by
    // mistake.
    memset(cal_params->offset, 0, sizeof(cal_params->offset));
  }
#endif  // OVERTEMPCAL_GYRO_ENABLED
}
#endif  // GYRO_CAL_ENABLED

// Detects and converts Factory Calibration data into a format consumable by the
// runtime magnetometer calibration algorithm.
#ifdef MAG_CAL_ENABLED
void HandleMagFactoryCalibration(struct ashCalParams *cal_params) {
  // Checks for factory calibration data and performs any processing on the
  // input to make it compatible with this runtime algorithm.
  bool factory_cal_detected =
      cal_params->offsetSource == ASH_CAL_PARAMS_SOURCE_NONE &&
      cal_params->offsetTempCelsiusSource == ASH_CAL_PARAMS_SOURCE_FACTORY;

  if (factory_cal_detected) {
    // Prints the received factory data.
    PrintAshCalParams(*cal_params,"[NanoSensorCal:MAG_FACTORY_CAL]");

    // Sets the parameter source to runtime calibration.
    cal_params->offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    cal_params->offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;

    // Ensures that the offset vector is zero in case it has been overwritten by
    // mistake.
    memset(cal_params->offset, 0, sizeof(cal_params->offset));
  }
}
#endif  // MAG_CAL_ENABLED

}  // anonymous namespace

NanoSensorCal::NanoSensorCal() {
  // Initializes the calibration data to a known default state.
  ResetCalParams(&accel_cal_params_);
  ResetCalParams(&gyro_cal_params_);
  ResetCalParams(&mag_cal_params_);
}

void NanoSensorCal::Initialize() {
  NANO_CAL_LOG("[NanoSensorCal]", "Initialized.");

#ifdef ACCEL_CAL_ENABLED
  // Initializes the accelerometer offset calibration algorithm.
  accelCalInit(&accel_cal_,
               800000000,       // Stillness Time in ns (0.8s)
               5,               // Minimum Sample Number
               0.00025f,        // Threshold
               15,              // nx bucket count
               15,              // nxb bucket count
               15,              // ny bucket count
               15,              // nyb bucket count
               15,              // nz bucket count
               15,              // nzb bucket count
               15);             // nle bucket count

  // Retrieves stored calibration data using the ASH API.
  LoadAshAccelCal();
#endif  // ACCEL_CAL_ENABLED

#ifdef GYRO_CAL_ENABLED
  // Initializes the gyroscope offset calibration algorithm.
  gyroCalInit(
      &gyro_cal_,
      2.0e9,                    // min stillness period = 2.0 seconds
      3.0e9,                    // max stillness period = 3.0 seconds
      0, 0, 0,                  // initial bias offset calibration
      0,                        // time stamp of initial bias calibration
      1.0e9,                    // analysis window length = 1.0 seconds
      7.5e-5f,                  // gyroscope variance threshold [rad/sec]^2
      1e-5f,                    // gyroscope confidence delta [rad/sec]^2
      8e-3f,                    // accelerometer variance threshold [m/sec^2]^2
      1.6e-3f,                  // accelerometer confidence delta [m/sec^2]^2
      5.0f,                     // magnetometer variance threshold [uT]^2
      1.0f,                     // magnetometer confidence delta [uT]^2
      0.95f,                    // stillness threshold [0,1]
      60.0e-3f * kPi / 180.0f,  // stillness mean variation limit [rad/sec]
      1.5f,   // maximum temperature deviation during stillness [C]
      true);  // gyro calibration enable

#ifdef OVERTEMPCAL_GYRO_ENABLED
  // Initializes the over-temperature compensated gyroscope (OTC-Gyro) offset
  // calibration algorithm.
  overTempCalInit(
      &over_temp_gyro_cal_,
      5,                        // Min num of points to enable model update
      5000000000,               // Min model update interval [nsec]
      0.75f,                    // Temperature span of bin method [C]
      100.0e-3f * kPi / 180.0f, // Model fit tolerance [rad/sec]
      250.0e-3f * kPi / 180.0f, // Outlier rejection tolerance [rad/sec]
      172800000000000,          // Model data point age limit [nsec]
      250.0e-3f * kPi / 180.0f, // Limit for temp. sensitivity [rad/sec/C]
      8.0f * kPi / 180.0f,      // Limit for model intercept parameter [rad/sec]
      0.5e-3f * kPi / 180.0f,   // Significant offset change [rad/sec]
      true);                    // Over-temp compensation enable
#endif  // OVERTEMPCAL_GYRO_ENABLED

  // Retrieves stored calibration data using the ASH API.
  LoadAshGyroCal();
#endif  // GYRO_CAL_ENABLED

#ifdef MAG_CAL_ENABLED
#ifdef DIVERSITY_CHECK_ENABLED
#ifdef SPHERE_FIT_ENABLED
  // Full Sphere Fit.
  // TODO: Replace function parameters with a struct, to avoid swapping them per
  // accident.
  initMagCalSphere(&mag_cal_sphere_,
                   0.0f, 0.0f, 0.0f,            // bias x, y, z.
                   1.0f, 0.0f, 0.0f,            // c00, c01, c02.
                   0.0f, 1.0f, 0.0f,            // c10, c11, c12.
                   0.0f, 0.0f, 1.0f,            // c20, c21, c22.
                   7357000,                     // min_batch_window_in_micros
                   15,                          // min_num_diverse_vectors.
                   1,                           // max_num_max_distance.
                   5.0f,                        // var_threshold.
                   8.0f,                        // max_min_threshold.
                   48.f,                        // local_field.
                   0.49f,                       // threshold_tuning_param.
                   2.5f);                       // max_distance_tuning_param.
  magCalSphereOdrUpdate(&mag_cal_sphere_, 50 /* Default sample rate Hz */);

  // ODR init.
  memset(&mag_sample_rate_data_, 0, sizeof(SampleRateData));
#endif  // SPHERE_FIT_ENABLED

  // Initializes the magnetometer offset calibration algorithm (with diversity
  // checker).
  initMagCal(&mag_cal_,
             0.0f, 0.0f, 0.0f,  // bias x, y, z
             1.0f, 0.0f, 0.0f,  // c00, c01, c02
             0.0f, 1.0f, 0.0f,  // c10, c11, c12
             0.0f, 0.0f, 1.0f,  // c20, c21, c22
             3000000,           // min_batch_window_in_micros
             8,                 // min_num_diverse_vectors
             1,                 // max_num_max_distance
             6.0f,              // var_threshold
             10.0f,             // max_min_threshold
             48.f,              // local_field
             0.49f,             // threshold_tuning_param
             2.5f);             // max_distance_tuning_param
#else
  // Initializes the magnetometer offset calibration algorithm.
  initMagCal(&mag_cal_,
             0.0f, 0.0f, 0.0f,   // bias x, y, z
             1.0f, 0.0f, 0.0f,   // c00, c01, c02
             0.0f, 1.0f, 0.0f,   // c10, c11, c12
             0.0f, 0.0f, 1.0f,   // c20, c21, c22
             3000000);           // min_batch_window_in_micros
#endif  // DIVERSITY_CHECK_ENABLED

  // Retrieves stored calibration data using the ASH API.
  LoadAshMagCal();
#endif  // MAG_CAL_ENABLED

  // Resets the calibration ready flags.
  accel_calibration_ready_ = false;
  gyro_calibration_ready_ = false;
  mag_calibration_ready_ = false;

  // NanoSensorCal algorithms have been initialized.
  nanosensorcal_initialized_ = true;
}

// TODO: Evaluate the impact of sensor batching on the performance of the
// calibration algorithms (versus processing on a per-sample basis). For
// example, some of the internal algorithms rely on the temperature signal to
// determine when temperature variation is too high to perform calibrations.
void NanoSensorCal::HandleSensorSamples(
    uint16_t event_type, const chreSensorThreeAxisData *event_data) {
  if (nanosensorcal_initialized_) {
    HandleSensorSamplesAccelCal(event_type, event_data);
    HandleSensorSamplesGyroCal(event_type, event_data);
    HandleSensorSamplesMagCal(event_type, event_data);
  }
}

void NanoSensorCal::HandleTemperatureSamples(
    uint16_t event_type, const chreSensorFloatData *event_data) {
  if (!nanosensorcal_initialized_)
      return;

  // Takes the mean of the batched temperature samples and delivers it to the
  // calibration algorithms. The latency setting determines the minimum update
  // interval.
  if (event_type == CHRE_EVENT_SENSOR_ACCELEROMETER_TEMPERATURE_DATA &&
      event_data->header.readingCount > 0) {
    const auto header = event_data->header;
    const auto *data = event_data->readings;
    uint64_t timestamp_nanos = header.baseTimestamp;
    float mean_temperature_celsius = 0.0f;
    for (size_t i = 0; i < header.readingCount; i++) {
      timestamp_nanos += data[i].timestampDelta;
      mean_temperature_celsius += data[i].value;
    }
    mean_temperature_celsius /= header.readingCount;
    temperature_celsius_ = mean_temperature_celsius;

#ifdef GYRO_CAL_ENABLED
#ifdef OVERTEMPCAL_GYRO_ENABLED
      // Updates the OTC gyro temperature.
      overTempCalSetTemperature(&over_temp_gyro_cal_, timestamp_nanos,
                                temperature_celsius_);
#endif  // OVERTEMPCAL_GYRO_ENABLED
#endif  // GYRO_CAL_ENABLED
  }
}

void NanoSensorCal::HandleSensorSamplesAccelCal(
    uint16_t event_type, const chreSensorThreeAxisData *event_data) {
#ifdef ACCEL_CAL_ENABLED
  if (event_type == CHRE_EVENT_SENSOR_UNCALIBRATED_ACCELEROMETER_DATA) {
    const auto header = event_data->header;
    const auto *data = event_data->readings;
    uint64_t timestamp_nanos = header.baseTimestamp;
    for (size_t i = 0; i < header.readingCount; i++) {
      timestamp_nanos += data[i].timestampDelta;
      accelCalRun(&accel_cal_, timestamp_nanos,
                  data[i].v[0],  // x-axis data [m/sec^2]
                  data[i].v[1],  // y-axis data [m/sec^2]
                  data[i].v[2],  // z-axis data [m/sec^2]
                  temperature_celsius_);
    }
    // Checks for an accelerometer bias calibration change.
    float offset[3] = {0.0f, 0.0f, 0.0f};
    if (accelCalUpdateBias(&accel_cal_, &offset[0], &offset[1], &offset[2])) {
      // Provides a new accelerometer calibration update.
      accel_calibration_ready_ = true;
      NotifyAshAccelCal();
    }

#ifdef ACCEL_CAL_DBG_ENABLED
    // Prints debug data report.
    accelCalDebPrint(&accel_cal_, temperature_celsius_);
#endif
  }
#endif  // ACCEL_CAL_ENABLED
}

// TODO: Factor common code to shorten function and improve readability.
void NanoSensorCal::HandleSensorSamplesGyroCal(
    uint16_t event_type, const chreSensorThreeAxisData *event_data) {
    uint64_t timestamp_nanos = 0;
#ifdef GYRO_CAL_ENABLED
  switch (event_type) {
    case CHRE_EVENT_SENSOR_UNCALIBRATED_ACCELEROMETER_DATA: {
      const auto header = event_data->header;
      const auto *data = event_data->readings;
      timestamp_nanos = header.baseTimestamp;
      for (size_t i = 0; i < header.readingCount; i++) {
        timestamp_nanos += data[i].timestampDelta;
        gyroCalUpdateAccel(&gyro_cal_, timestamp_nanos,
                           data[i].v[0],   // x-axis data [m/sec^2]
                           data[i].v[1],   // y-axis data [m/sec^2]
                           data[i].v[2]);  // z-axis data [m/sec^2]
      }
      break;
    }

    case CHRE_EVENT_SENSOR_UNCALIBRATED_GYROSCOPE_DATA: {
      const auto header = event_data->header;
      const auto *data = event_data->readings;
      timestamp_nanos = header.baseTimestamp;
      for (size_t i = 0; i < header.readingCount; i++) {
        timestamp_nanos += data[i].timestampDelta;
        gyroCalUpdateGyro(&gyro_cal_, timestamp_nanos,
                          data[i].v[0],  // x-axis data [rad/sec]
                          data[i].v[1],  // y-axis data [rad/sec]
                          data[i].v[2],  // z-axis data [rad/sec]
                          temperature_celsius_);
      }

      if (gyroCalNewBiasAvailable(&gyro_cal_)) {
#ifdef OVERTEMPCAL_GYRO_ENABLED
        // Sends new GyroCal offset estimate to the OTC-Gyro.
        float offset[3] = {0.0f, 0.0f, 0.0f};
        float offset_temperature_celsius = 0.0f;
        gyroCalGetBias(&gyro_cal_, &offset[0], &offset[1], &offset[2],
                       &offset_temperature_celsius);
        overTempCalUpdateSensorEstimate(&over_temp_gyro_cal_, timestamp_nanos,
                                        offset, offset_temperature_celsius);
#else
        // Provides a new gyroscope calibration update.
        gyro_calibration_ready_ = true;
        NotifyAshGyroCal();
#endif  // OVERTEMPCAL_GYRO_ENABLED
      }

#ifdef OVERTEMPCAL_GYRO_ENABLED
      // Limits the frequency of the offset update checks.
      if ((timestamp_nanos >=
           otc_offset_timer_nanos_ + kOtcGyroOffsetMaxUpdateIntervalNanos) ||
          (timestamp_nanos < otc_offset_timer_nanos_)) {
        otc_offset_timer_nanos_ = timestamp_nanos;

        // Checks OTC for new calibration model update.
        bool new_otc_model_update =
            overTempCalNewModelUpdateAvailable(&over_temp_gyro_cal_);

        // Checks for a change in the OTC-Gyro temperature compensated offset
        // estimate.
        bool new_otc_offset =
            overTempCalNewOffsetAvailable(&over_temp_gyro_cal_);

        if (new_otc_model_update || new_otc_offset) {
          // Provides a temperature compensated gyroscope calibration update.
          gyro_calibration_ready_ = true;
          NotifyAshGyroCal();
        }
      }
#endif  // OVERTEMPCAL_GYRO_ENABLED
      break;
    }

    case CHRE_EVENT_SENSOR_UNCALIBRATED_GEOMAGNETIC_FIELD_DATA: {
      const auto header = event_data->header;
      const auto *data = event_data->readings;
      timestamp_nanos = header.baseTimestamp;
      for (size_t i = 0; i < header.readingCount; i++) {
        timestamp_nanos += data[i].timestampDelta;
        gyroCalUpdateMag(&gyro_cal_, timestamp_nanos,
                         data[i].v[0],   // x-axis data [uT]
                         data[i].v[1],   // y-axis data [uT]
                         data[i].v[2]);  // z-axis data [uT]
      }
      break;
    }

    default:
      break;
  }

  if (timestamp_nanos > 0) {
#ifdef GYRO_CAL_DBG_ENABLED
    // Prints debug data report.
    gyroCalDebugPrint(&gyro_cal_, timestamp_nanos);
#endif  // GYRO_CAL_DBG_ENABLED

#if defined(OVERTEMPCAL_GYRO_ENABLED) && defined(OVERTEMPCAL_DBG_ENABLED)
    // Prints debug data report.
    overTempCalDebugPrint(&over_temp_gyro_cal_, timestamp_nanos);
#endif  // OVERTEMPCAL_GYRO_ENABLED && OVERTEMPCAL_DBG_ENABLED
  }
#endif  // GYRO_CAL_ENABLED
}

void NanoSensorCal::HandleSensorSamplesMagCal(
    uint16_t event_type, const chreSensorThreeAxisData *event_data) {
#ifdef MAG_CAL_ENABLED
  if (event_type == CHRE_EVENT_SENSOR_UNCALIBRATED_GEOMAGNETIC_FIELD_DATA) {
    const auto header = event_data->header;
    const auto *data = event_data->readings;
    uint64_t timestamp_nanos = header.baseTimestamp;
    MagUpdateFlags new_calibration_update_mag_cal = MagUpdate::NO_UPDATE;

    for (size_t i = 0; i < header.readingCount; i++) {
      timestamp_nanos += data[i].timestampDelta;

      // Sets the flag to indicate a new calibration update.
      new_calibration_update_mag_cal |= magCalUpdate(
          &mag_cal_,
          static_cast<uint64_t>(timestamp_nanos * kNanoToMicroseconds),
          data[i].v[0],   // x-axis data [uT]
          data[i].v[1],   // y-axis data [uT]
          data[i].v[2]);  // z-axis data [uT]

#ifdef SPHERE_FIT_ENABLED
      // Sphere Fit Algo Part.

      // getting ODR.
      if (mag_sample_rate_data_.num_samples <
           kSamplesToAverageForOdrEstimateMag) {
        SamplingRateEstimate(&mag_sample_rate_data_, nullptr, timestamp_nanos,
                             false);
      } else {
        SamplingRateEstimate(&mag_sample_rate_data_, &mag_odr_estimate_hz_,
                             0, true);

        // Sphere fit ODR update.
        magCalSphereOdrUpdate(&mag_cal_sphere_, mag_odr_estimate_hz_);
      }

      // Running Sphere fit, and getting trigger.
      new_calibration_update_mag_cal |= magCalSphereUpdate(
          &mag_cal_sphere_,
          static_cast<uint64_t>(timestamp_nanos * kNanoToMicroseconds),
          data[i].v[0],   // x-axis data [uT]
          data[i].v[1],   // y-axis data [uT]
          data[i].v[2]);  // z-axis data [uT]
#endif  // SPHERE_FIT_ENABLED
    }

    if ((MagUpdate::UPDATE_BIAS & new_calibration_update_mag_cal) ||
        (MagUpdate::UPDATE_SPHERE_FIT & new_calibration_update_mag_cal)) {
      // Sets the flag to indicate a new calibration update is pending.
      mag_calibration_ready_ = true;
      NotifyAshMagCal(new_calibration_update_mag_cal);
    }
  }
#endif  // MAG_CAL_ENABLED
}

void NanoSensorCal::GetAccelerometerCalibration(
    struct ashCalParams *accel_cal_params) const {
  // Resets the calibration ready flag; and returns the calibration data.
  accel_calibration_ready_ = false;
  memcpy(accel_cal_params, &accel_cal_params_, sizeof(struct ashCalParams));
}

void NanoSensorCal::GetGyroscopeCalibration(
    struct ashCalParams *gyro_cal_params) const {
  // Resets the calibration ready flag; and returns the calibration data.
  gyro_calibration_ready_ = false;
  memcpy(gyro_cal_params, &gyro_cal_params_, sizeof(struct ashCalParams));
}

void NanoSensorCal::GetMagnetometerCalibration(
    struct ashCalParams *mag_cal_params) const {
  // Resets the calibration ready flag; and returns the calibration data.
  mag_calibration_ready_ = false;
  memcpy(mag_cal_params, &mag_cal_params_, sizeof(struct ashCalParams));
}

void NanoSensorCal::UpdateAccelCalParams() {
#ifdef ACCEL_CAL_ENABLED
  // Gets the accelerometer's offset vector and temperature.
  accelCalUpdateBias(&accel_cal_, &accel_cal_params_.offset[0],
                     &accel_cal_params_.offset[1], &accel_cal_params_.offset[2]);
  accel_cal_params_.offsetTempCelsius = temperature_celsius_;

  // Sets the parameter source to runtime calibration.
  accel_cal_params_.offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
  accel_cal_params_.offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
#endif  // ACCEL_CAL_ENABLED
}

void NanoSensorCal::UpdateGyroCalParams() {
#ifdef GYRO_CAL_ENABLED
#ifdef OVERTEMPCAL_GYRO_ENABLED
  // Gets the gyroscope's offset vector and temperature; and OTC linear model
  // parameters.
  uint64_t timestamp_nanos = 0;
  overTempCalGetModel(&over_temp_gyro_cal_, gyro_cal_params_.offset,
                      &gyro_cal_params_.offsetTempCelsius, &timestamp_nanos,
                      gyro_cal_params_.tempSensitivity,
                      gyro_cal_params_.tempIntercept);

  // Sets the parameter source to runtime calibration.
  gyro_cal_params_.offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
  gyro_cal_params_.offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
  gyro_cal_params_.tempSensitivitySource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
  gyro_cal_params_.tempInterceptSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
#else
  // Gets the gyroscope's offset vector and temperature.
  gyroCalGetBias(&gyro_cal_, &gyro_cal_params_.offset[0],
                 &gyro_cal_params_.offset[1], &gyro_cal_params_.offset[2],
                 &gyro_cal_params_.offsetTempCelsius);

  // Sets the parameter source to runtime calibration.
  gyro_cal_params_.offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
  gyro_cal_params_.offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
#endif  // OVERTEMPCAL_GYRO_ENABLED
#endif  // GYRO_CAL_ENABLED
}

void NanoSensorCal::UpdateMagCalParams(MagUpdateFlags new_update) {
#ifdef MAG_CAL_ENABLED
  if (MagUpdate::UPDATE_SPHERE_FIT & new_update) {
#ifdef SPHERE_FIT_ENABLED
    // Updating the mag offset from sphere fit.
    mag_cal_params_.offset[0] = mag_cal_sphere_.sphere_fit.sphere_param.bias[0];
    mag_cal_params_.offset[1] = mag_cal_sphere_.sphere_fit.sphere_param.bias[1];
    mag_cal_params_.offset[2] = mag_cal_sphere_.sphere_fit.sphere_param.bias[2];

    // Updating the Sphere Param.
    mag_cal_params_.scaleFactor[0] =
        mag_cal_sphere_.sphere_fit.sphere_param.scale_factor_x;
    mag_cal_params_.scaleFactor[1] =
        mag_cal_sphere_.sphere_fit.sphere_param.scale_factor_y;
    mag_cal_params_.scaleFactor[2] =
        mag_cal_sphere_.sphere_fit.sphere_param.scale_factor_z;
    mag_cal_params_.crossAxis[0] =
        mag_cal_sphere_.sphere_fit.sphere_param.skew_yx;
    mag_cal_params_.crossAxis[1] =
        mag_cal_sphere_.sphere_fit.sphere_param.skew_zx;
    mag_cal_params_.crossAxis[2] =
        mag_cal_sphere_.sphere_fit.sphere_param.skew_zy;

    // Updating the temperature.
    mag_cal_params_.offsetTempCelsius = temperature_celsius_;

    // Sets the parameter source to runtime calibration.
    mag_cal_params_.offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    mag_cal_params_.scaleFactorSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    mag_cal_params_.crossAxisSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    mag_cal_params_.offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
#endif  // SPHERE_FIT_ENABLED
  } else if (MagUpdate::UPDATE_BIAS & new_update) {
    // Gets the magnetometer's offset vector and temperature.
    magCalGetBias(&mag_cal_, &mag_cal_params_.offset[0],
                  &mag_cal_params_.offset[1], &mag_cal_params_.offset[2]);
    mag_cal_params_.offsetTempCelsius = temperature_celsius_;

    // Sets the parameter source to runtime calibration.
    mag_cal_params_.offsetSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
    mag_cal_params_.offsetTempCelsiusSource = ASH_CAL_PARAMS_SOURCE_RUNTIME;
  }
#endif  // MAG_CAL_ENABLED
}

void NanoSensorCal::LoadAshAccelCal() {
#ifdef ACCEL_CAL_ENABLED
  bool load_successful = false;
  struct ashCalParams cal_params;
  if (ashLoadCalibrationParams(CHRE_SENSOR_TYPE_ACCELEROMETER,
                               ASH_CAL_STORAGE_ASH, &cal_params)) {
    // Checks for and performs required processing on input factory cal data.
    HandleAccelFactoryCalibration(&cal_params);

    // Checks for valid calibration data.
    if (cal_params.offsetSource == ASH_CAL_PARAMS_SOURCE_RUNTIME &&
        cal_params.offsetTempCelsiusSource == ASH_CAL_PARAMS_SOURCE_RUNTIME) {
      // On a successful load, copies the new set of calibration parameters.
      memcpy(&accel_cal_params_, &cal_params, sizeof(struct ashCalParams));

      // Sets the accelerometer algorithm's calibration data.
      accelCalBiasSet(&accel_cal_, accel_cal_params_.offset[0],
                      accel_cal_params_.offset[1], accel_cal_params_.offset[2]);
      load_successful = true;
    }
  }

  if (load_successful) {
    // Updates the calibration data with ASH.
    NotifyAshAccelCal();

    // Prints recalled calibration data.
    NANO_CAL_LOG("[NanoSensorCal:RECALL ACCEL]",
                 "Offset [m/sec^2] | Temp [Celsius]: %.6f, %.6f, %.6f | %.6f",
                 accel_cal_params_.offset[0], accel_cal_params_.offset[1],
                 accel_cal_params_.offset[2],
                 accel_cal_params_.offsetTempCelsius);
  } else {
    NANO_CAL_LOG("[NanoSensorCal:RECALL ACCEL]",
                 "Failed to recall accelerometer calibration data from "
                 "persistent memory.");
  }
#endif  // ACCEL_CAL_ENABLED
}

void NanoSensorCal::LoadAshGyroCal() {
#ifdef GYRO_CAL_ENABLED
  bool load_successful = false;
  struct ashCalParams cal_params;
  if (ashLoadCalibrationParams(CHRE_SENSOR_TYPE_GYROSCOPE,
                               ASH_CAL_STORAGE_ASH, &cal_params)) {
    // Checks for and performs required processing on input factory cal data.
    HandleGyroFactoryCalibration(&cal_params);

#ifdef OVERTEMPCAL_GYRO_ENABLED
    // Gyroscope offset calibration with over-temperature compensation (OTC)
    // parameters were recalled.
    if (cal_params.offsetSource == ASH_CAL_PARAMS_SOURCE_RUNTIME &&
        cal_params.offsetTempCelsiusSource == ASH_CAL_PARAMS_SOURCE_RUNTIME &&
        cal_params.tempSensitivitySource == ASH_CAL_PARAMS_SOURCE_RUNTIME &&
        cal_params.tempInterceptSource == ASH_CAL_PARAMS_SOURCE_RUNTIME) {
      // On a successful load, copies the new set of calibration parameters.
      memcpy(&gyro_cal_params_, &cal_params, sizeof(struct ashCalParams));

      // Sets the gyroscope algorithm's calibration data.
      const uint64_t timestamp_nanos = chreGetTime();
      gyroCalSetBias(&gyro_cal_, gyro_cal_params_.offset[0],
                     gyro_cal_params_.offset[1], gyro_cal_params_.offset[2],
                     timestamp_nanos);
      overTempCalSetModel(&over_temp_gyro_cal_,
                          gyro_cal_params_.offset,
                          gyro_cal_params_.offsetTempCelsius,
                          timestamp_nanos,
                          gyro_cal_params_.tempSensitivity,
                          gyro_cal_params_.tempIntercept,
                          /*jump_start_model=*/true);
      load_successful = true;

      // Prints recalled calibration data.
      NANO_CAL_LOG("[NanoSensorCal:RECALL OTC-GYRO]",
                   "Offset [rad/sec] | Temp [Celsius]: %.6f, %.6f, %.6f | %.6f",
                   gyro_cal_params_.offset[0], gyro_cal_params_.offset[1],
                   gyro_cal_params_.offset[2],
                   gyro_cal_params_.offsetTempCelsius);
      NANO_CAL_LOG("[NanoSensorCal:RECALL OTC-GYRO]",
                   "Sensitivity [rad/sec/C] | Intercept [rad/sec]: %.6f, %.6f, "
                   "%.6f | %.6f, %.6f, %.6f",
                   gyro_cal_params_.tempSensitivity[0],
                   gyro_cal_params_.tempSensitivity[1],
                   gyro_cal_params_.tempSensitivity[2],
                   gyro_cal_params_.tempIntercept[0],
                   gyro_cal_params_.tempIntercept[1],
                   gyro_cal_params_.tempIntercept[2]);
    }
#else
    // Gyroscope offset calibration parameters were recalled.
    if (cal_params.offsetSource == ASH_CAL_PARAMS_SOURCE_RUNTIME &&
        cal_params.offsetTempCelsiusSource == ASH_CAL_PARAMS_SOURCE_RUNTIME) {
      // On a successful load, copies the new set of calibration parameters.
      memcpy(&gyro_cal_params_, &cal_params, sizeof(struct ashCalParams));

      // Sets the gyroscope algorithm's calibration data.
      gyroCalSetBias(&gyro_cal_, gyro_cal_params_.offset[0],
                     gyro_cal_params_.offset[1], gyro_cal_params_.offset[2],
                     /*calibration_time_nanos=*/0);
      load_successful = true;

      // Prints recalled calibration data.
      NANO_CAL_LOG("[NanoSensorCal:RECALL GYRO]",
                   "Offset [rad/sec] | Temp [Celsius]: %.6f, %.6f, %.6f | %.6f",
                   gyro_cal_params_.offset[0], gyro_cal_params_.offset[1],
                   gyro_cal_params_.offset[2],
                   gyro_cal_params_.offsetTempCelsius);
    }
#endif  // OVERTEMPCAL_GYRO_ENABLED
  }

  if (load_successful) {
    // Updates the calibration data with ASH.
    NotifyAshGyroCal();
  } else {
    NANO_CAL_LOG(
        "[NanoSensorCal:RECALL GYRO]",
        "Failed to recall gyroscope calibration data from persistent memory.");
  }
#endif  // GYRO_CAL_ENABLED
}

void NanoSensorCal::LoadAshMagCal() {
#ifdef MAG_CAL_ENABLED
  bool load_successful = false;
  struct ashCalParams cal_params;
  if (ashLoadCalibrationParams(CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD,
                               ASH_CAL_STORAGE_ASH, &cal_params)) {
    // Checks for and performs required processing on input factory cal data.
    HandleMagFactoryCalibration(&cal_params);

    // Checks for valid calibration data.
    if (cal_params.offsetSource == ASH_CAL_PARAMS_SOURCE_RUNTIME &&
        cal_params.offsetTempCelsiusSource == ASH_CAL_PARAMS_SOURCE_RUNTIME) {
      // On a successful load, copies the new set of calibration parameters.
      memcpy(&mag_cal_params_, &cal_params, sizeof(struct ashCalParams));

      // Sets the magnetometer algorithm's calibration data.
      magCalReset(&mag_cal_);  // Resets the magnetometer's offset vector.
      magCalAddBias(&mag_cal_, mag_cal_params_.offset[0],
                    mag_cal_params_.offset[1], mag_cal_params_.offset[2]);

#ifdef SPHERE_FIT_ENABLED
      // Sets Sphere Fit calibration data.
      mag_cal_sphere_.sphere_fit.sphere_param.scale_factor_x =
          mag_cal_params_.scaleFactor[0];
      mag_cal_sphere_.sphere_fit.sphere_param.scale_factor_y =
          mag_cal_params_.scaleFactor[1];
      mag_cal_sphere_.sphere_fit.sphere_param.scale_factor_z =
          mag_cal_params_.scaleFactor[2];
      mag_cal_sphere_.sphere_fit.sphere_param.skew_yx =
          mag_cal_params_.crossAxis[0];
      mag_cal_sphere_.sphere_fit.sphere_param.skew_zx =
          mag_cal_params_.crossAxis[1];
      mag_cal_sphere_.sphere_fit.sphere_param.skew_zy =
          mag_cal_params_.crossAxis[2];
      mag_cal_sphere_.sphere_fit.sphere_param.bias[0] =
          mag_cal_params_.offset[0];
      mag_cal_sphere_.sphere_fit.sphere_param.bias[1] =
          mag_cal_params_.offset[1];
      mag_cal_sphere_.sphere_fit.sphere_param.bias[2] =
          mag_cal_params_.offset[2];
#endif  // SPHERE_FIT_ENABLED
      load_successful = true;
    }
  }

  if (load_successful) {
    // Updates the calibration data with ASH.
#ifdef SPHERE_FIT_ENABLED
    NotifyAshMagCal(MagUpdate::UPDATE_SPHERE_FIT);
#else
    NotifyAshMagCal(MagUpdate::UPDATE_BIAS);
#endif  // SPHERE_FIT_ENABLED

    // Prints recalled calibration data.
    NANO_CAL_LOG("[NanoSensorCal:RECALL MAG]",
                 "Offset [uT] | Temp [Celsius] : %.3f, %.3f, %.3f | %.3f",
                 mag_cal_params_.offset[0], mag_cal_params_.offset[1],
                 mag_cal_params_.offset[2], mag_cal_params_.offsetTempCelsius);
#ifdef SPHERE_FIT_ENABLED
    NANO_CAL_LOG("[NanoSensorCal:RECALL MAG]",
                 "Scale Factor [%] | Cross Axis [%]: %.3f, %.3f, %.3f |"
                 " %.3f, %.3f, %.3f",
                 mag_cal_params_.scaleFactor[0], mag_cal_params_.scaleFactor[1],
                 mag_cal_params_.scaleFactor[2], mag_cal_params_.crossAxis[0],
                 mag_cal_params_.crossAxis[1], mag_cal_params_.crossAxis[2]);
#endif  // SPHERE_FIT_ENABLED
  } else {
    NANO_CAL_LOG("[NanoSensorCal:RECALL MAG]",
                 "Failed to recall Magnetometer calibration data from "
                 "persistent memory.");
  }
#endif  // MAG_CAL_ENABLED
}

void NanoSensorCal::NotifyAshAccelCal() {
#ifdef ACCEL_CAL_ENABLED
  // Update ASH with the latest calibration data.
  UpdateAccelCalParams();
  struct ashCalInfo cal_info;
  ResetCalInfo(&cal_info);
  memcpy(cal_info.bias, accel_cal_params_.offset, sizeof(cal_info.bias));
  cal_info.accuracy = ASH_CAL_ACCURACY_HIGH;
  ashSetCalibration(CHRE_SENSOR_TYPE_ACCELEROMETER, &cal_info);

  NANO_CAL_LOG("[NanoSensorCal:UPDATE ACCEL]",
               "Offset [m/sec^2] | Temp [Celsius]: %.6f, %.6f, %.6f | %.6f",
               accel_cal_params_.offset[0], accel_cal_params_.offset[1],
               accel_cal_params_.offset[2],
               accel_cal_params_.offsetTempCelsius);

  // TODO: Remove #if FACTORYCAL_IS_VERIFIED directives, which currently prevent
  // saving calibration parameter updates to the sensor registry, after Factory
  // Calibration testing has been fully qualified on hardware.
#define FACTORYCAL_IS_VERIFIED 1  // Set to 0, blocks overwriting factory data.
#if FACTORYCAL_IS_VERIFIED
  // Store the calibration parameters using the ASH API.
  if (!ashSaveCalibrationParams(CHRE_SENSOR_TYPE_ACCELEROMETER,
                               &accel_cal_params_)) {
    NANO_CAL_LOG("[NanoSensorCal:STORE ACCEL]",
                 "ASH failed to write calibration update.");
  }
#endif  // FACTORYCAL_IS_VERIFIED
#endif  // ACCEL_CAL_ENABLED
}

void NanoSensorCal::NotifyAshGyroCal() {
#ifdef GYRO_CAL_ENABLED
  // Update ASH with the latest calibration data.
  UpdateGyroCalParams();
  struct ashCalInfo cal_info;
  ResetCalInfo(&cal_info);
  memcpy(cal_info.bias, gyro_cal_params_.offset, sizeof(cal_info.bias));
  cal_info.accuracy = ASH_CAL_ACCURACY_HIGH;
  ashSetCalibration(CHRE_SENSOR_TYPE_GYROSCOPE, &cal_info);

#ifdef OVERTEMPCAL_GYRO_ENABLED
  NANO_CAL_LOG("[NanoSensorCal:UPDATE OTC-GYRO]",
               "Offset [rad/sec] | Temp [Celsius]: %.6f, %.6f, %.6f | %.6f",
               gyro_cal_params_.offset[0], gyro_cal_params_.offset[1],
               gyro_cal_params_.offset[2], gyro_cal_params_.offsetTempCelsius);
  NANO_CAL_LOG(
      "[NanoSensorCal:UPDATE OTC-GYRO]",
      "Sensitivity [rad/sec/C] | Intercept [rad/sec]: %.6f, %.6f, "
      "%.6f | %.6f, %.6f, %.6f",
      gyro_cal_params_.tempSensitivity[0], gyro_cal_params_.tempSensitivity[1],
      gyro_cal_params_.tempSensitivity[2], gyro_cal_params_.tempIntercept[0],
      gyro_cal_params_.tempIntercept[1], gyro_cal_params_.tempIntercept[2]);
#else
  NANO_CAL_LOG("[NanoSensorCal:UPDATE GYRO]",
               "Offset [rad/sec] | Temp [Celsius]: %.6f, %.6f, %.6f | %.6f",
               gyro_cal_params_.offset[0], gyro_cal_params_.offset[1],
               gyro_cal_params_.offset[2], gyro_cal_params_.offsetTempCelsius);
#endif  // OVERTEMPCAL_GYRO_ENABLED

  // TODO: Remove #if FACTORYCAL_IS_VERIFIED directives, which currently prevent
  // saving calibration parameter updates to the sensor registry, after Factory
  // Calibration testing has been fully qualified on hardware.
#if FACTORYCAL_IS_VERIFIED
  // Store the calibration parameters using the ASH API.
  if (!ashSaveCalibrationParams(CHRE_SENSOR_TYPE_GYROSCOPE, &gyro_cal_params_)) {
    NANO_CAL_LOG("[NanoSensorCal:STORE GYRO]",
                 "ASH failed to write calibration update.");
  }
#endif  // FACTORYCAL_IS_VERIFIED
#endif  // GYRO_CAL_ENABLED
}

void NanoSensorCal::NotifyAshMagCal(MagUpdateFlags new_update) {
#ifdef MAG_CAL_ENABLED
  // Update ASH with the latest calibration data.
  UpdateMagCalParams(new_update);
  struct ashCalInfo cal_info;
  ResetCalInfo(&cal_info);
  memcpy(cal_info.bias, mag_cal_params_.offset, sizeof(cal_info.bias));

  // TODO: Adding Sphere Parameters to compensation matrix.
  cal_info.accuracy = ASH_CAL_ACCURACY_HIGH;
  ashSetCalibration(CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD, &cal_info);

  NANO_CAL_LOG("[NanoSensorCal:UPDATE MAG]",
               "Offset [uT] | Temp [Celsius]: %.6f, %.6f, %.6f | %.6f",
               mag_cal_params_.offset[0], mag_cal_params_.offset[1],
               mag_cal_params_.offset[2], mag_cal_params_.offsetTempCelsius);
#ifdef SPHERE_FIT_ENABLED
  NANO_CAL_LOG("[NanoSensorCal:UPDATE MAG]",
               "Scale Factor [%] | Cross Axis [%]: %.3f, %.3f, %.3f | "
               " %.3f, %.3f, %.3f",
               mag_cal_params_.scaleFactor[0], mag_cal_params_.scaleFactor[1],
               mag_cal_params_.scaleFactor[2], mag_cal_params_.crossAxis[0],
               mag_cal_params_.crossAxis[1], mag_cal_params_.crossAxis[2]);
#endif  // SPHERE_FIT_ENABLED

  // TODO: Remove #if FACTORYCAL_IS_VERIFIED directives, which currently prevent
  // saving calibration parameter updates to the sensor registry, after Factory
  // Calibration testing has been fully qualified on hardware.
#if FACTORYCAL_IS_VERIFIED
  // Store the calibration parameters using the ASH API.
  if (!ashSaveCalibrationParams(CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD,
                               &mag_cal_params_)) {
    NANO_CAL_LOG("[NanoSensorCal:STORE MAG]",
                 "ASH failed to write calibration update.");
  }
#endif  // FACTORYCAL_IS_VERIFIED
#endif  // MAG_CAL_ENABLED
}

}  // namespace nano_calibration
