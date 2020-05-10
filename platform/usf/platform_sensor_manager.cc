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

#include "chre/platform/platform_sensor_manager.h"

#include "chre/core/event_loop_manager.h"

namespace chre {
namespace {

void addSensor(usf::UsfServerHandle sensorHandle, uint8_t sensorType,
               const usf::UsfMsgSensorInfoResp *sensorInfoResp,
               DynamicVector<Sensor> *chreSensors) {
  uint64_t minInterval = SensorTypeHelpers::isOneShot(sensorType)
                             ? CHRE_SENSOR_INTERVAL_DEFAULT
                             : sensorInfoResp->min_delay_ns();
  const char *sensorName =
      reinterpret_cast<const char *>(sensorInfoResp->name()->data());
  LOGI("%s, type %" PRIu8 ", minInterval %" PRIu64 " handle %" PRId32,
       sensorName, sensorType, minInterval, sensorHandle);

  if (!chreSensors->emplace_back()) {
    FATAL_ERROR("Failed to allocate new sensor");
  }

  // The sensor base class must be initialized before the main Sensor init()
  // can be invoked as init() is allowed to invoke base class methods.
  chreSensors->back().initBase(sensorHandle, sensorType, minInterval,
                               sensorName);
  chreSensors->back().init();
}

void addSensorsWithInfo(usf::UsfServerHandle sensorHandle,
                        const usf::UsfMsgSensorInfoResp *sensorInfoResp,
                        DynamicVector<Sensor> *chreSensors) {
  uint8_t sensorType;
  if (convertUsfToChreSensorType(sensorInfoResp->type(), &sensorType)) {
    addSensor(sensorHandle, sensorType, sensorInfoResp, chreSensors);

    // USF shares the same sensor type for calibrated and uncalibrated sensors
    // so populate a calibrated / uncalibrated sensor for known calibrated
    // sensor types
    uint8_t calibratedType =
        PlatformSensorTypeHelpersBase::toUncalibratedSensorType(sensorType);
    if (calibratedType != sensorType) {
      addSensor(sensorHandle, calibratedType, sensorInfoResp, chreSensors);
    }

    // USF shares a sensor type for both gyro and accel temp.
    if (sensorType == CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE) {
      addSensor(sensorHandle, CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE,
                sensorInfoResp, chreSensors);
    }
  }
}

ReportingMode getReportingMode(Sensor &sensor) {
  if (sensor.isOneShot()) {
    return ReportingMode::OneShot;
  } else if (sensor.isOnChange()) {
    return ReportingMode::OnChange;
  } else {
    return ReportingMode::Continuous;
  }
}

void handleMissingSensor() {
  // Try rebooting if a sensor is missing, which might help recover from a
  // transient failure/race condition at startup. But to avoid endless crashes,
  // only do this within 45 seconds of boot time - we rely on knowledge that
  // getMonotonicTime() only resets when the device reboots and not from an SSR.
#ifndef CHRE_LOG_ONLY_NO_SENSOR
  if (SystemTime::getMonotonicTime() < (kDefaultUsfWaitTimeout + Seconds(15))) {
    FATAL_ERROR("Missing required sensor(s)");
  } else
#endif
  {
    LOGE("Missing required sensor(s)");
  }
}

}  // namespace

PlatformSensorManager::~PlatformSensorManager() {}

void PlatformSensorManager::init() {
  mHelper.init(this);
}

DynamicVector<Sensor> PlatformSensorManager::getSensors() {
  DynamicVector<Sensor> sensors;

  usf::UsfReqSyncCallback callback;
  const usf::UsfMsgSensorListResp *list;
  if (mHelper.getSensorList(callback, &list)) {
    for (size_t i = 0; i < list->handle_list()->size(); i++) {
      usf::UsfServerHandle serverHandle = list->handle_list()->Get(i);

      usf::UsfReqSyncCallback infoCallback;
      const usf::UsfMsgSensorInfoResp *info;
      if (mHelper.getSensorInfo(serverHandle, infoCallback, &info)) {
        addSensorsWithInfo(serverHandle, info, &sensors);
      }
    }
  }

  // TODO: Determine how many sensors are expected to be available through USF.
  if (sensors.empty()) {
    handleMissingSensor();
  }

  return sensors;
}

bool PlatformSensorManager::configureSensor(Sensor &sensor,
                                            const SensorRequest &request) {
  bool success = false;

  if (request.getMode() == SensorMode::Off) {
    usf::UsfStopSamplingReq usfReq;
    usfReq.SetReqType(usf::UsfMsgReqType_STOP_SAMPLING);
    usfReq.SetServerHandle(sensor.getServerHandle());
    usfReq.SetSamplingId(sensor.getSamplingId());

    if (mHelper.stopSampling(&usfReq)) {
      sensor.setSamplingIdInvalid();
      success = true;
    }
  } else if (sensor.isSamplingIdValid()) {
    // TODO: See if a reconfigure request can be the same as reissuing a start
    // request.
    usf::UsfReconfigSamplingReq usfReq;
    usfReq.SetReqType(usf::UsfMsgReqType_RECONFIG_SAMPLING);
    usfReq.SetServerHandle(sensor.getServerHandle());
    usfReq.SetSamplingId(sensor.getSamplingId());
    usfReq.SetPeriodNs(request.getInterval().toRawNanoseconds());
    usfReq.SetMaxLatencyNs(
        sensor.isContinuous() ? request.getLatency().toRawNanoseconds() : 0);

    success = mHelper.reconfigureSampling(&usfReq);
  } else {
    usf::UsfStartSamplingReq usfReq;
    usfReq.SetReqType(usf::UsfMsgReqType_START_SAMPLING);
    usfReq.SetServerHandle(sensor.getServerHandle());
    // TODO(147438885): Make setting the reporting mode of a sensor optional
    usfReq.SetReportingMode(getUsfReportingMode(getReportingMode(sensor)));
    usfReq.SetPeriodNs(request.getInterval().toRawNanoseconds());
    usfReq.SetMaxLatencyNs(
        sensor.isContinuous() ? request.getLatency().toRawNanoseconds() : 0);
    usfReq.SetPassive(sensorModeIsPassive(request.getMode()));
    usf::UsfSensorTransformLevel transformLevel =
        sensor.isCalibrated() ? usf::kUsfSensorTransformLevelAll
                              : usf::kUsfSensorTransformEnvCompensation;
    usfReq.SetTransformLevel(transformLevel);

    uint32_t samplingId;
    if (mHelper.startSampling(&usfReq, &samplingId)) {
      sensor.setSamplingId(samplingId);
      success = true;
    }
  }

  return success;
}

bool PlatformSensorManager::configureBiasEvents(const Sensor & /* sensor */,
                                                bool /* enable */,
                                                uint64_t /* latencyNs */) {
  return true;
}

bool PlatformSensorManager::getThreeAxisBias(
    const Sensor &sensor, struct chreSensorThreeAxisData *bias) const {
  return false;
}

bool PlatformSensorManager::flush(const Sensor &sensor,
                                  uint32_t *flushRequestId) {
  return false;
}

void PlatformSensorManager::releaseSamplingStatusUpdate(
    struct chreSensorSamplingStatus *status) {
  memoryFree(status);
}

void PlatformSensorManager::releaseSensorDataEvent(void *data) {
  memoryFree(data);
}

void PlatformSensorManager::releaseBiasEvent(void *biasData) {
  memoryFree(biasData);
}

bool PlatformSensorManagerBase::getSensorInfo(uint8_t sensorType,
                                              SensorInfo *sensorInfo) {
  bool success = false;

  if (getSensorRequestManager().getSensorHandle(sensorType,
                                                &sensorInfo->sensorHandle)) {
    Sensor *sensor =
        getSensorRequestManager().getSensor(sensorInfo->sensorHandle);
    if (sensor != nullptr && sensor->isSamplingIdValid()) {
      success = true;
      sensorInfo->samplingId = sensor->getSamplingId();
    } else {
      sensorInfo->sensorHandle = 0;
    }
  }
  return success;
}

void PlatformSensorManagerBase::onSensorDataEvent(
    uint8_t sensorType, UniquePtr<uint8_t> &&eventData) {
  uint32_t sensorHandle;
  if (getSensorRequestManager().getSensorHandle(sensorType, &sensorHandle)) {
    getSensorRequestManager().handleSensorDataEvent(sensorHandle,
                                                    eventData.release());
  }
}

}  // namespace chre
