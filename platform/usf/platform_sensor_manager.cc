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
#include "chre/util/nested_data_ptr.h"

namespace chre {
namespace {

void addSensor(refcount::reffed_ptr<usf::UsfSensor> &usfSensor,
               uint8_t sensorType, DynamicVector<Sensor> *chreSensors) {
  if (!chreSensors->emplace_back()) {
    FATAL_ERROR("Failed to allocate new sensor");
  }

  // The sensor base class must be initialized before the main Sensor init()
  // can be invoked as init() is allowed to invoke base class methods.
  chreSensors->back().initBase(usfSensor, sensorType);
  chreSensors->back().init();

  const Sensor &newSensor = chreSensors->back();
  LOGI("%s, type %" PRIu8 ", minInterval %" PRIu64 " handle %" PRId32,
       newSensor.getSensorName(), newSensor.getSensorType(),
       newSensor.getMinInterval(), newSensor.getServerHandle());
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
  if (!mHelper.registerForApPowerStateUpdates()) {
    LOGE("Failed to register for AP power state updates.");
  }
}

DynamicVector<Sensor> PlatformSensorManager::getSensors() {
  DynamicVector<Sensor> sensors;

  usf::UsfVector<refcount::reffed_ptr<usf::UsfSensor>> sensorList;
  if (mHelper.getSensorList(&sensorList)) {
    for (size_t i = 0; i < sensorList.size(); i++) {
      refcount::reffed_ptr<usf::UsfSensor> &sensor = sensorList[i];
      addSensorsWithInfo(sensor, &sensors);
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

  if (request.getMode() == SensorMode::Off && !sensor.isSamplingIdValid()) {
    // If no existing sampling ID exists and the sensor should be turned off,
    // return true. This can happen when the core platform tries to remove a
    // request for a one-shot sensor after it fires which automatically happens
    // inside USF.
    success = true;
  } else if (request.getMode() == SensorMode::Off) {
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
    usfReq.SetReportingMode(PlatformSensorTypeHelpersBase::getUsfReportingMode(
        getReportingMode(sensor)));
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

bool PlatformSensorManager::configureBiasEvents(const Sensor &sensor,
                                                bool enable,
                                                uint64_t /* latencyNs */) {
  bool success = true;
  if (enable) {
    success = mHelper.registerForBiasUpdates(sensor.getUsfSensor());
  } else {
    // Unconditional success for unregistering
    mHelper.unregisterForBiasUpdates(sensor.getUsfSensor());
  }

  return success;
}

bool PlatformSensorManager::getThreeAxisBias(
    const Sensor &sensor, struct chreSensorThreeAxisData *bias) const {
  bool success = sensor.reportsBiasEvents();
  if (success) {
    if (!mHelper.getThreeAxisBias(sensor.getUsfSensor(), bias)) {
      // Set to zero bias + unknown accuracy per CHRE API requirements.
      memset(bias, 0, sizeof(chreSensorThreeAxisData));
      bias->header.readingCount = 1;
      bias->header.accuracy = CHRE_SENSOR_ACCURACY_UNKNOWN;
    }

    // Overwrite sensorHandle to match the request type.
    getSensorRequestManager().getSensorHandle(sensor.getSensorType(),
                                              &bias->header.sensorHandle);
  }
  return success;
}

bool PlatformSensorManager::flush(const Sensor &sensor,
                                  uint32_t *flushRequestId) {
  return mHelper.flushAsync(
      sensor.getServerHandle(),
      NestedDataPtr<uint32_t>(sensor.getSensorType()) /* cookie */,
      flushRequestId);
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

void PlatformSensorManagerBase::onSamplingStatusUpdate(
    uint8_t sensorType, const usf::UsfSensorSamplingEvent *update) {
  uint32_t sensorHandle;
  if (getSensorRequestManager().getSensorHandle(sensorType, &sensorHandle)) {
    // Memory will be freed via releaseSamplingStatusUpdate once core framework
    // performs its updates.
    auto statusUpdate = MakeUniqueZeroFill<struct chreSensorSamplingStatus>();
    if (statusUpdate.isNull()) {
      LOG_OOM();
    } else {
      statusUpdate->interval = update->GetPeriodNs();
      statusUpdate->latency = update->GetMaxLatencyNs();
      statusUpdate->enabled = update->GetActive();
      getSensorRequestManager().handleSamplingStatusUpdate(
          sensorHandle, statusUpdate.release());
    }
  }
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
    Sensor *sensor = getSensorRequestManager().getSensor(sensorHandle);
    // Invalidate the sampling ID for one shot sensors after they've fired since
    // USF automatically removes the sampling ID from its list.
    if (sensor->isOneShot()) {
      sensor->setSamplingIdInvalid();
    }

    getSensorRequestManager().handleSensorDataEvent(sensorHandle,
                                                    eventData.release());
  }
}

void PlatformSensorManagerBase::onFlushComplete(usf::UsfErr err,
                                                uint32_t requestId,
                                                void *cookie) {
  uint32_t sensorType = NestedDataPtr<uint32_t>(cookie);
  uint32_t sensorHandle;
  if (getSensorRequestManager().getSensorHandle(sensorType, &sensorHandle)) {
    getSensorRequestManager().handleFlushCompleteEvent(
        sensorHandle, requestId, UsfHelper::usfErrorToChreError(err));
  }
}

void PlatformSensorManagerBase::onBiasUpdate(
    uint8_t sensorType, UniquePtr<struct chreSensorThreeAxisData> &&eventData) {
  uint32_t sensorHandle;
  if (getSensorRequestManager().getSensorHandle(sensorType, &sensorHandle)) {
    eventData->header.sensorHandle = sensorHandle;
    getSensorRequestManager().handleBiasEvent(sensorHandle,
                                              eventData.release());
  }
}

void PlatformSensorManagerBase::onHostWakeSuspendEvent(bool awake) {
  EventLoopManagerSingleton::get()
      ->getEventLoop()
      .getPowerControlManager()
      .onHostWakeSuspendEvent(awake);
}

void PlatformSensorManagerBase::addSensorsWithInfo(
    refcount::reffed_ptr<usf::UsfSensor> &usfSensor,
    DynamicVector<Sensor> *chreSensors) {
  uint8_t sensorType;
  if (PlatformSensorTypeHelpersBase::convertUsfToChreSensorType(
          usfSensor->GetType(), &sensorType)) {
    // Only register for a USF sensor once. If it maps to multiple sensors,
    // code down the line will handle sending multiple updates.
    if (!mHelper.registerForStatusUpdates(usfSensor)) {
      LOGE("Failed to register for status updates for %s",
           usfSensor->GetName());
    }

    addSensor(usfSensor, sensorType, chreSensors);

    // USF shares the same sensor type for calibrated and uncalibrated sensors
    // so populate a calibrated / uncalibrated sensor for known calibrated
    // sensor types
    uint8_t uncalibratedType =
        SensorTypeHelpers::toUncalibratedSensorType(sensorType);
    if (uncalibratedType != sensorType) {
      addSensor(usfSensor, uncalibratedType, chreSensors);
    }

    // USF shares a sensor type for both gyro and accel temp.
    if (sensorType == CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE) {
      addSensor(usfSensor, CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE,
                chreSensors);
    }
  }
}

}  // namespace chre
