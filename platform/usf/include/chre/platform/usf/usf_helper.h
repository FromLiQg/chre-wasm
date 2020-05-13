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

#ifndef CHRE_PLATFORM_USF_USF_HELPER_H_
#define CHRE_PLATFORM_USF_USF_HELPER_H_

#include "chre/core/sensor_type.h"
#include "chre/platform/system_time.h"
#include "chre/util/unique_ptr.h"

#include "usf/error.h"
#include "usf/fbs/usf_msg_sample_root_generated.h"
#include "usf/reffed_ptr.h"
#include "usf/usf_sensor.h"
#include "usf/usf_sensor_req.h"
#include "usf/usf_transport_client.h"

namespace chre {

struct SensorInfo {
  //! Used to fill in the chreSensorDataHeader field. If the user of the helper
  //! doesn't care about this field, then it can be set to 0.
  uint32_t sensorHandle;

  //! Sampling ID corresponding to the active subscription to this sensor, if
  //! one exists. 0 otherwise.
  uint32_t samplingId;
};

//! A callback interface for receiving UsfHelper data events and to allow the
//! UsfHelper to ask for information.
class UsfHelperCallbackInterface {
 public:
  //! Invoked from the USF worker thread to provide sensor data events. The
  //! event data format is one of the chreSensorXXXData defined in the CHRE API,
  //! implicitly specified by sensorType.
  virtual void onSensorDataEvent(uint8_t sensorType,
                                 UniquePtr<uint8_t> &&eventData) = 0;

  /**
   * Invoked by the USF Helper to obtain sensor info for a given sensor type.
   *
   * @param sensorType The sensor type for which info should be populated
   * @param sensorInfo Non-null pointer that will be populated with sensor info
   *     if this method is successful
   * @return true if all sensor info was obtained.
   */
  virtual bool getSensorInfo(uint8_t sensorType, SensorInfo *sensorInfo) = 0;

  //! Invoked from the USF worker thread to provide a sensor sampling status
  //! update. All fields are guaranteed to be valid.
  virtual void onSamplingStatusUpdate(
      uint8_t sensorType, const usf::UsfSensorSamplingEvent *update) = 0;
};

//! Class that allows clients of UsfHelper that only want to listen for sensor
//! events to implement only functions that relate to those events.
class SensorEventCallbackInterface : public UsfHelperCallbackInterface {
 public:
  void onSamplingStatusUpdate(
      uint8_t /*sensorType*/,
      const usf::UsfSensorSamplingEvent * /*update*/) override {}
};

//! Default timeout to wait for the USF transport client to return a response
//! to a request
constexpr Nanoseconds kDefaultUsfWaitTimeout = Seconds(5);

/**
 * Helper class used to abstract away most details of communicating with USF.
 */
class UsfHelper {
 public:
  ~UsfHelper();

  /**
   * Initializes connection to USF.
   *
   * @param callback USF helper callback.
   * @param worker USF worker pointer. If null, a new worker is created.
   */
  void init(UsfHelperCallbackInterface *callback,
            usf::UsfWorker *worker = nullptr);

  /**
   * Retrieves the list of sensors available from USF.
   *
   * @param sensorList Non-null pointer to a UsfVector that will be populated
   *     with all the available sensors from USF.
   * @return true if the list of sensors was retrieved successfully.
   */
  bool getSensorList(
      usf::UsfVector<refcount::reffed_ptr<usf::UsfSensor>> *sensorList);

  /**
   * Starts sampling sensor data from the given sensor.
   *
   * @param request The various parameters needed to issue the request to the
   *     sensor.
   * @param samplingId Non-null pointer that will be filled in with a valid
   *     sampling ID if the request is issued successfully.
   * @return true if the sensor request was issued successfully.
   */
  bool startSampling(usf::UsfStartSamplingReq *request, uint32_t *samplingId);

  /**
   * Reconfigures an existing sensor request.
   *
   * @param request The various parameters needed to issue the request to the
   *     sensor.
   * @return true if the sensor request was reconfigured successfully.
   */
  bool reconfigureSampling(usf::UsfReconfigSamplingReq *request);

  /**
   * Stops an active sensor request.
   *
   * @param request The various parameters needed to issue the request to the
   *     sensor.
   * @return true if the sensor request was stopped successfully.
   */
  bool stopSampling(usf::UsfStopSamplingReq *request);

  /**
   * Registers the helper for sensor sampling status updates from USF for the
   * given sensor.
   *
   * @param sensor The sensor to listen to sampling status updates from.
   * @return true if registration was successful.
   */
  bool registerForStatusUpdates(refcount::reffed_ptr<usf::UsfSensor> &sensor);

  /**
   * Used to process sensor samples delivered through a listener registered with
   * USF.
   *
   * @param event Pointer containing a valid sensor sample
   */
  void processSensorSample(const usf::UsfMsgEvent *event);

  /**
   * Used to process a sensor sampling status update delivered through a
   * listener registered with USF.
   *
   * @param update Update containing the latest state from USF.
   */
  void processStatusUpdate(const usf::UsfSensorSamplingEvent *update);

 private:
  /**
   * Disconnects from underlying services prior to shutdown
   */
  void deinit();

  /**
   * Sends a synchronous request to USF with any returned data stored in the
   * given callback.
   *
   * @param req Request that should be issued to USF
   * @param callback Helper callback function used to perform the synchronous
   *     request and contains any response message after the method returns
   * @return true if no errors were encountered issuing the request
   */
  bool sendUsfReqSync(usf::UsfReq *req, usf::UsfReqSyncCallback *callback);

  /**
   * Retrieves the response message from the synchronous callback. The memory
   * associated with the message is owned by the callback itself and will be
   * freed upon callback going out of scope.
   *
   * @param callback Synchronous callback previously used in a successful
   *     sendUsfReqSync invocation
   * @param respMsg Variable containing the response message if this method
   *     succeeds
   * @return true if the response message was decoded and verified successfully
   *     and respMsg is valid.
   */
  template <class T>
  bool getRespMsg(usf::UsfReqSyncCallback &callback, const T **respMsg);

  /**
   * Creates a CHRE sensor event from a USF sensor event
   *
   * @param sampleMsg Valid decoded USF sensor event
   * @param sensorType CHRE sensor type corresponding to the event
   * @param sensorSample Upon success, populated with a valid CHRE sensor event
   * @return true if the USF sensor event corresponds to a valid, active
   *     sampling request and event creation was successful
   */
  bool createSensorEvent(const usf::UsfMsgSampleBatch *sampleMsg,
                         uint8_t sensorType, UniquePtr<uint8_t> &sensorSample);

  //! Client used to send messages to USF
  refcount::reffed_ptr<usf::UsfTransportClient> mTransportClient;

  //! UsfWorker used to dispatch messages to CHRE on its own thread
  refcount::reffed_ptr<usf::UsfWorker> mWorker;

  //! All registered USF event listeners.
  DynamicVector<usf::UsfEventListener *> mUsfEventListeners;

  //! Handle to the USF sensor manager
  usf::UsfServerHandle mSensorMgrHandle;

  //! Currently registered callback
  UsfHelperCallbackInterface *mCallback = nullptr;
};

}  // namespace chre

#endif  // CHRE_PLATFORM_USF_USF_HELPER_H_
