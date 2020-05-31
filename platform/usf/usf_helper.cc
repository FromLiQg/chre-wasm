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

#include "chre/platform/usf/usf_helper.h"

#include <cinttypes>

#include "chre/platform/platform_sensor_type_helpers.h"

#include "usf/fbs/usf_msg_event_root_generated.h"
#include "usf/fbs/usf_msg_sample_batch_root_generated.h"
#include "usf/fbs/usf_msg_sensor_info_resp_root_generated.h"
#include "usf/fbs/usf_msg_sensor_list_resp_root_generated.h"
#include "usf/fbs/usf_msg_start_sampling_resp_root_generated.h"
#include "usf/usf.h"
#include "usf/usf_device.h"
#include "usf/usf_flatbuffers.h"
#include "usf/usf_sensor_defs.h"
#include "usf/usf_sensor_mgr.h"
#include "usf/usf_work.h"

using usf::UsfErr;
using usf::UsfErr::kErrNone;

#define LOG_USF_ERR(x) LOGE("USF failure %" PRIu8 ": line %d", x, __LINE__)

namespace chre {
namespace {

//! Callback data delivered to async callback from USF. This struct must be
//! freed using memoryFree.
struct AsyncCallbackData {
  UsfHelperCallbackInterface *callback;
  void *cookie;
};

void eventHandler(void *context, usf::UsfEvent *event) {
  const usf::UsfMsgEvent *msgEvent =
      static_cast<usf::UsfTransportMsgEvent *>(event)->GetMsgEvent();

  if (msgEvent != nullptr) {
    auto *mgr = static_cast<UsfHelper *>(context);

    switch (msgEvent->event_type()) {
      case usf::UsfMsgEventType_kSample:
        mgr->processSensorSample(msgEvent);
        break;
      default:
        LOGD("Received unknown event type %" PRIu8, msgEvent->event_type());
        break;
    }
  }
}

void statusUpdateHandler(void *context, usf::UsfEvent *event) {
  const usf::UsfSensorSamplingEvent *updateEvent =
      static_cast<usf::UsfSensorSamplingEvent *>(event);

  if (updateEvent != nullptr) {
    auto *mgr = static_cast<UsfHelper *>(context);
    mgr->processStatusUpdate(updateEvent);
  }
}

void asyncCallback(usf::UsfReq *req, const usf::UsfResp *resp, void *data) {
  auto callbackData = static_cast<AsyncCallbackData *>(data);
  callbackData->callback->onFlushComplete(resp->GetErr(), req->GetReqId(),
                                          callbackData->cookie);

  memoryFree(data);
  memoryFree(req);
}

void *allocateEvent(uint8_t sensorType, size_t numSamples) {
  SensorSampleType sampleType =
      PlatformSensorTypeHelpers::getSensorSampleTypeFromSensorType(sensorType);
  size_t sampleSize = 0;
  switch (sampleType) {
    case SensorSampleType::ThreeAxis:
      sampleSize =
          sizeof(chreSensorThreeAxisData::chreSensorThreeAxisSampleData);
      break;
    case SensorSampleType::Float:
      sampleSize = sizeof(chreSensorFloatData::chreSensorFloatSampleData);
      break;
    case SensorSampleType::Byte:
      sampleSize = sizeof(chreSensorByteData::chreSensorByteSampleData);
      break;
    case SensorSampleType::Occurrence:
      sampleSize =
          sizeof(chreSensorOccurrenceData::chreSensorOccurrenceSampleData);
      break;
    default:
      LOGE("Unhandled SensorSampleType for SensorType %" PRIu8,
           static_cast<uint8_t>(sensorType));
      sampleType = SensorSampleType::Unknown;
  }

  size_t memorySize =
      (sampleType == SensorSampleType::Unknown)
          ? 0
          : (sizeof(chreSensorDataHeader) + numSamples * sampleSize);
  void *event = (memorySize == 0) ? nullptr : memoryAlloc(memorySize);

  if (event == nullptr && memorySize != 0) {
    LOG_OOM();
  }
  return event;
}

/**
 * Allocates the memory and sets up the chreSensorDataHeader used to deliver
 * the sensor samples to nanoapps.
 *
 * @param numSamples The number of samples there should be space for in the
 *     sensor event
 * @param sensorHandle The handle to the sensor that produced the events
 * @param sensor The CHRE sensor instance for the sensor that produced the
 *     events
 * @param sensorSamples A non-null UniquePtr used to hold the sensor samples
 * @return If successful, sensorSamples will point to valid memory that
 *     contains a valid chreSensorDataHeader and enough space to store all
 *     sensor samples
 */
bool prepareSensorEvent(size_t numSamples, uint32_t sensorHandle,
                        uint8_t sensorType, UniquePtr<uint8_t> &sensorSamples) {
  bool success = false;

  UniquePtr<uint8_t> buf(
      static_cast<uint8_t *>(allocateEvent(sensorType, numSamples)));
  sensorSamples = std::move(buf);

  if (!sensorSamples.isNull()) {
    success = true;

    auto *header =
        reinterpret_cast<chreSensorDataHeader *>(sensorSamples.get());
    header->sensorHandle = sensorHandle;
    header->readingCount = numSamples;
    // TODO(b/150308661): Populate this
    header->accuracy = CHRE_SENSOR_ACCURACY_UNKNOWN;
    header->reserved = 0;
  }

  return success;
}

/**
 * Populates CHRE sensor sample structure using USF sensor samples.
 *
 * @param sampleMsg The USF message that contains sensor samples
 * @param sensor The CHRE sensor instance for the sensor that produced the
 *     events
 * @param sensorSample Reference to a UniquePtr instance that points to non-null
 *     memory that will be populated with sensor samples
 */
void populateSensorEvent(const usf::UsfMsgSampleBatch *sampleMsg,
                         uint8_t sensorType, UniquePtr<uint8_t> &sensorSample) {
  uint64_t prevSampleTimeNs = 0;

  for (size_t i = 0; i < sampleMsg->sample_list()->size(); i++) {
    const float *sensorVal =
        sampleMsg->sample_list()->Get(i)->samples()->data();

    SensorSampleType sampleType =
        PlatformSensorTypeHelpers::getSensorSampleTypeFromSensorType(
            sensorType);

    uint32_t *timestampDelta = nullptr;
    switch (sampleType) {
      case SensorSampleType::ThreeAxis: {
        auto *event =
            reinterpret_cast<chreSensorThreeAxisData *>(sensorSample.get());

        // TODO(b/143139477): Apply calibration to data from these sensors
        for (size_t valIndex = 0; valIndex < 3; valIndex++) {
          event->readings[i].values[valIndex] = sensorVal[valIndex];
        }
        timestampDelta = &event->readings[i].timestampDelta;
        break;
      }
      case SensorSampleType::Float: {
        auto *event =
            reinterpret_cast<chreSensorFloatData *>(sensorSample.get());
        event->readings[i].value = sensorVal[0];
        timestampDelta = &event->readings[i].timestampDelta;
        break;
      }
      case SensorSampleType::Byte: {
        auto *event =
            reinterpret_cast<chreSensorByteData *>(sensorSample.get());
        event->readings[i].value = 0;
        event->readings[i].isNear = (sensorVal[0] > 0.5f);
        timestampDelta = &event->readings[i].timestampDelta;
        break;
      }
      case SensorSampleType::Occurrence: {
        // Occurrence samples don't store any readings
        auto *event =
            reinterpret_cast<chreSensorOccurrenceData *>(sensorSample.get());
        timestampDelta = &event->readings[0].timestampDelta;
        break;
      }
      default:
        LOGE("Invalid sample type %" PRIu8, static_cast<uint8_t>(sampleType));
    }

    // First sample determines the base timestamp for all other sensor samples
    uint64_t timestampNsHigh =
        sampleMsg->sample_list()->Get(i)->timestamp_ns_hi();
    uint64_t sampleTimestampNs =
        (timestampNsHigh << 32) |
        sampleMsg->sample_list()->Get(i)->timestamp_ns_lo();

    if (i == 0) {
      auto *header =
          reinterpret_cast<chreSensorDataHeader *>(sensorSample.get());
      header->baseTimestamp = sampleTimestampNs;
      if (timestampDelta != nullptr) {
        *timestampDelta = 0;
      }
    } else {
      uint64_t delta = sampleTimestampNs - prevSampleTimeNs;
      if (delta > UINT32_MAX) {
        LOGE("Sensor %" PRIu8 " timestampDelta overflow: prev %" PRIu64
             " curr %" PRIu64,
             sensorType, prevSampleTimeNs, sampleTimestampNs);
        delta = UINT32_MAX;
      }
      *timestampDelta = static_cast<uint32_t>(delta);
    }

    prevSampleTimeNs = sampleTimestampNs;
  }
}

}  // namespace

UsfHelper::~UsfHelper() {
  deinit();
}

uint8_t UsfHelper::usfErrorToChreError(UsfErr err) {
  switch (err) {
    case UsfErr::kErrNone:
      return CHRE_ERROR_NONE;
    case UsfErr::kErrNotSupported:
      return CHRE_ERROR_NOT_SUPPORTED;
    case UsfErr::kErrMemAlloc:
      return CHRE_ERROR_NO_MEMORY;
    case UsfErr::kErrNotAvailable:
      return CHRE_ERROR_BUSY;
    case UsfErr::kErrTimedOut:
      return CHRE_ERROR_TIMEOUT;
    default:
      return CHRE_ERROR;
  }
}

void UsfHelper::init(UsfHelperCallbackInterface *callback,
                     usf::UsfWorker *worker) {
  usf::UsfDeviceMgr::GetDeviceProbeCompletePrecondition()->Wait();

  UsfErr err = kErrNone;
  if (worker != nullptr) {
    mWorker.reset(worker, refcount::RefTakingMode::kCreate);
  } else {
    err = usf::UsfWorkMgr::CreateWorker(&mWorker);
  }

  if (!mUsfEventListeners.emplace_back()) {
    LOG_OOM();
    deinit();
  } else if ((err != kErrNone) ||
             ((err = usf::UsfTransportClient::Create(&mTransportClient)) !=
              kErrNone) ||
             ((err = mTransportClient->Connect()) != kErrNone) ||
             ((err = mTransportClient->GetMsgEventType()->AddListener(
                   mWorker.get(), eventHandler, this,
                   &mUsfEventListeners.back())) != kErrNone) ||
             ((err = usf::UsfClientGetServer(mTransportClient.get(),
                                             &usf::kUsfSensorMgrServerUuid,
                                             &mSensorMgrHandle)) != kErrNone)) {
    LOG_USF_ERR(err);
    deinit();
  } else {
    mCallback = callback;
  }
}

bool UsfHelper::getSensorList(
    usf::UsfVector<refcount::reffed_ptr<usf::UsfSensor>> *sensorList) {
  UsfErr err = usf::UsfSensorMgr::GetSensorList(sensorList);
  if (err != kErrNone) {
    LOG_USF_ERR(err);
  }
  return err == kErrNone;
}

void UsfHelper::deinit() {
  for (usf::UsfEventListener *listener : mUsfEventListeners) {
    listener->event_type->RemoveListener(listener);
  }

  if (mWorker.get() != nullptr) {
    mWorker->Stop();
    mWorker.reset();
  }

  if (mTransportClient.get() != nullptr) {
    mTransportClient->Disconnect();
    mTransportClient.reset();
  }
}

bool UsfHelper::startSampling(usf::UsfStartSamplingReq *request,
                              uint32_t *samplingId) {
  bool success = false;

  usf::UsfReqSyncCallback callback;
  const usf::UsfMsgStartSamplingResp *resp;
  if (sendUsfReqSync(request, &callback) && getRespMsg(callback, &resp)) {
    *samplingId = resp->sampling_id();
    success = true;
  }

  return success;
}

bool UsfHelper::reconfigureSampling(usf::UsfReconfigSamplingReq *request) {
  usf::UsfReqSyncCallback callback;
  return sendUsfReqSync(request, &callback);
}

bool UsfHelper::stopSampling(usf::UsfStopSamplingReq *request) {
  usf::UsfReqSyncCallback callback;
  return sendUsfReqSync(request, &callback);
}

bool UsfHelper::registerForStatusUpdates(
    refcount::reffed_ptr<usf::UsfSensor> &sensor) {
  bool success = false;
  if (!mUsfEventListeners.emplace_back()) {
    LOG_OOM();
  } else {
    UsfErr err = sensor->GetSamplingEventType()->AddListener(
        mWorker.get(), statusUpdateHandler, this, &mUsfEventListeners.back());
    success = err == kErrNone;
    if (!success) {
      LOG_USF_ERR(err);
    }
  }

  return success;
}

bool UsfHelper::flushAsync(const usf::UsfServerHandle usfSensorHandle,
                           void *cookie, uint32_t *requestId) {
  bool success = false;
  auto req = MakeUnique<usf::UsfReq>();
  auto callbackData = MakeUnique<AsyncCallbackData>();
  if (req.isNull() || callbackData.isNull()) {
    LOG_OOM();
  } else {
    req->SetReqType(usf::UsfMsgReqType_FLUSH_SAMPLES);
    req->SetServerHandle(usfSensorHandle);
    callbackData->callback = mCallback;
    callbackData->cookie = cookie;

    UsfErr err = mTransportClient->SendRequest(req.get(), asyncCallback,
                                               callbackData.get());
    success = (err == kErrNone);
    if (!success) {
      LOG_USF_ERR(err);
    } else {
      *requestId = req->GetReqId();
      req.release();
      callbackData.release();
    }
  }

  return success;
}

void UsfHelper::processSensorSample(const usf::UsfMsgEvent *event) {
  const usf::UsfMsgSampleBatch *sampleMsg;
  UniquePtr<uint8_t> sensorSample;
  uint8_t sensorType;

  UsfErr err;
  if ((err = usf::UsfMsgGet(event->data(), usf::GetUsfMsgSampleBatch,
                            usf::VerifyUsfMsgSampleBatchBuffer, &sampleMsg)) !=
      kErrNone) {
    LOG_USF_ERR(err);
  } else if (sampleMsg->sample_list()->size() == 0) {
    LOGE("Received empty sample batch");
  } else {
    auto usfType = static_cast<usf::UsfSensorType>(sampleMsg->sensor_type());

    if (PlatformSensorTypeHelpersBase::convertUsfToChreSensorType(
            usfType, &sensorType) &&
        !createSensorEvent(sampleMsg, sensorType, sensorSample)) {
      // USF shares the same sensor type between calibrated and uncalibrated
      // sensors. Try the uncalibrated type to see if the sampling ID matches.
      uint8_t uncalType =
          PlatformSensorTypeHelpers::toUncalibratedSensorType(sensorType);
      if (uncalType != sensorType) {
        sensorType = uncalType;
        createSensorEvent(sampleMsg, uncalType, sensorSample);
        // USF shares a sensor type for both gyro and accel temp.
      } else if (sensorType == CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE) {
        createSensorEvent(sampleMsg, CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE,
                          sensorSample);
      }
    }
  }

  if (!sensorSample.isNull() && mCallback != nullptr) {
    mCallback->onSensorDataEvent(sensorType, std::move(sensorSample));
  }
}

void UsfHelper::processStatusUpdate(const usf::UsfSensorSamplingEvent *update) {
  // TODO(stange): See if the latest status can be shared among sensor types
  // with the same underlying physical sensor to avoid allocating multiple
  // updates.
  uint8_t sensorType;
  if (PlatformSensorTypeHelpersBase::convertUsfToChreSensorType(
          update->GetSensor()->GetType(), &sensorType)) {
    mCallback->onSamplingStatusUpdate(sensorType, update);

    // USF shares the same sensor type for calibrated and uncalibrated sensors
    // so populate a calibrated / uncalibrated sensor for known calibrated
    // sensor types
    uint8_t uncalibratedType =
        PlatformSensorTypeHelpersBase::toUncalibratedSensorType(sensorType);
    if (uncalibratedType != sensorType) {
      mCallback->onSamplingStatusUpdate(uncalibratedType, update);
    }

    // USF shares a sensor type for both gyro and accel temp.
    if (sensorType == CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE) {
      mCallback->onSamplingStatusUpdate(
          CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE, update);
    }
  }
}

bool UsfHelper::createSensorEvent(const usf::UsfMsgSampleBatch *sampleMsg,
                                  uint8_t sensorType,
                                  UniquePtr<uint8_t> &sensorSample) {
  bool success = false;

  SensorInfo info;
  if (mCallback != nullptr && mCallback->getSensorInfo(sensorType, &info) &&
      (info.samplingId == sampleMsg->sampling_id())) {
    if (prepareSensorEvent(sampleMsg->sample_list()->size(), info.sensorHandle,
                           sensorType, sensorSample)) {
      populateSensorEvent(sampleMsg, sensorType, sensorSample);
      success = true;
    }
  }

  return success;
}

bool UsfHelper::sendUsfReqSync(usf::UsfReq *req,
                               usf::UsfReqSyncCallback *callback) {
  UsfErr err = mTransportClient->SendRequest(
      req, usf::UsfReqSyncCallback::Callback, callback);
  if (err != kErrNone) {
    LOG_USF_ERR(err);
  } else {
    // TODO: Provide a different timeout for operations we expect to take longer
    // e.g. initial sensor discovery.
    callback->Wait(kDefaultUsfWaitTimeout.toRawNanoseconds());
    if ((err = callback->GetResp()->GetErr()) != kErrNone) {
      LOG_USF_ERR(err);
    }
  }

  return err == kErrNone;
}

template <class T>
bool UsfHelper::getRespMsg(usf::UsfReqSyncCallback &callback,
                           const T **respMsg) {
  UsfErr err = usf::UsfRespGetMsg(callback.GetResp(), respMsg);
  if (err != kErrNone) {
    LOG_USF_ERR(err);
  }

  return err == kErrNone;
}

}  // namespace chre
