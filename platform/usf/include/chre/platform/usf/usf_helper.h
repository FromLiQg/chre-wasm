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
#include "chre/util/dynamic_vector.h"
#include "chre/util/unique_ptr.h"

/*
 * Include USF FlatBuffers services header before including other USF
 * FlatBuffers headers.
 */
#include "usf/usf_flatbuffers.h"

#include "usf/error.h"
#include "usf/fbs/usf_msg_sample_root_generated.h"
#include "usf/reffed_ptr.h"
#include "usf/usf_power.h"
#include "usf/usf_sensor.h"
#include "usf/usf_sensor_report.h"
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

  //! Invoked from the USF worker thread to indicate that flushing the given
  //! sensor has completed.
  virtual void onFlushComplete(usf::UsfErr err, uint32_t requestId,
                               void *cookie) = 0;

  //! Invoked from the USF worker thread to provide a bias update.
  virtual void onBiasUpdate(
      uint8_t sensorType,
      UniquePtr<struct chreSensorThreeAxisData> &&eventData) = 0;

  //! Invoked from the USF worker thread to provide an host wake suspend update.
  virtual void onHostWakeSuspendEvent(bool awake) = 0;
};

//! Class that allows clients of UsfHelper that only want to listen for sensor
//! events to implement only functions that relate to those events.
class SensorEventCallbackInterface : public UsfHelperCallbackInterface {
 public:
  void onSamplingStatusUpdate(
      uint8_t /*sensorType*/,
      const usf::UsfSensorSamplingEvent * /*update*/) override {}

  void onFlushComplete(usf::UsfErr /*err*/, uint32_t /*requestId*/,
                       void * /*cookie*/) override {}

  void onBiasUpdate(
      uint8_t /*sensorType*/,
      UniquePtr<struct chreSensorThreeAxisData> && /*eventData*/) override {}

  void onHostWakeSuspendEvent(bool /*awake*/) override {}
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

  //! Maps USF error codes to CHRE error codes.
  static uint8_t usfErrorToChreError(usf::UsfErr err);

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
   * Registers the helper for bias updates from USF for the given sensor.
   * This method is not idempotent meaning a call to unregisterForBiasUpdates
   * must be made for the same sensor prior to another call to this method.
   *
   * @param sensor A non-null reffed_ptr to the sensor to listen for bias
   *     updates from.
   * @return true if registration was successful.
   */
  bool registerForBiasUpdates(
      const refcount::reffed_ptr<usf::UsfSensor> &sensor);

  /**
   * Registers the helper for AP power state update from USF.
   *
   * @return true if successful.
   */
  bool registerForApPowerStateUpdates();

  /**
   * Unregisters the helper from bias updates from USF for the given sensor.
   * If not registered, this method is a no-op.
   *
   * @param sensor A non-null reffed_ptr to the sensor to stop listening to bias
   *     updates from.
   */
  void unregisterForBiasUpdates(
      const refcount::reffed_ptr<usf::UsfSensor> &sensor);

  /**
   * Obtains the latest three axis bias received from the given sensor, if
   * available.
   *
   * @param sensor The USF sensor to obtain the latest bias values from.
   * @param bias A non-null pointer that will be filled with the latest bias
   *     values if available.
   * @return true if a bias value was available for the given sensor.
   */
  bool getThreeAxisBias(const refcount::reffed_ptr<usf::UsfSensor> &sensor,
                        struct chreSensorThreeAxisData *bias) const;

  /**
   * Flushes sensor data from the provided sensor asynchronously. Once all data
   * has been flushed, {@link #onFlushCompleteEvent} will be invoked on the
   * callback the helper was initialized with.
   *
   * @param usfSensorHandle The USF handle corresponding to the sensor to flush
   * @param cookie A cookie that will be delivered when the flush has completed
   * @param requestId The ID associated with this flush request, if it was
   *     made successfully
   * @return True if the flush request was successfully sent.
   */
  bool flushAsync(const usf::UsfServerHandle usfSensorHandle, void *cookie,
                  uint32_t *requestId);

  /**
   * Used to process sensor reports delivered through a listener registered with
   * USF.
   *
   * @param event Pointer containing a valid sensor report
   */
  void processSensorReport(const usf::UsfMsgEvent *event);

  /**
   * Used to process a sensor sampling status update delivered through a
   * listener registered with USF.
   *
   * @param update Update containing the latest state from USF.
   */
  void processStatusUpdate(const usf::UsfSensorSamplingEvent *update);

  /**
   * Process the bias update delivered through a listener registered with USF.
   *
   * @param update Update containing latest bias information for a sensor.
   */
  void processBiasUpdate(const usf::UsfSensorTransformConfigEvent *update);

  /**
   * Process the AP power state update delivered through a  listener registered
   * with USF.
   *
   * @param update Update containing the current AP power state.
   */
  void processApPowerStateUpdate(const usf::UsfApPowerStateEvent *update);

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
   * @param sampleReport Valid decoded USF sensor sample report
   * @param sensorType CHRE sensor type corresponding to the event
   * @param sensorSample Upon success, populated with a valid CHRE sensor event
   * @return true if the USF sensor event corresponds to a valid, active
   *     sampling request and event creation was successful
   */
  bool createSensorEvent(const usf::UsfSensorSampleReport *sampleReport,
                         uint8_t sensorType, UniquePtr<uint8_t> &sensorSample);

  /**
   * Converts a USF bias update event into the chreSensorThreeAxisData format.
   *
   * @param update USF bias update that needs to be converted.
   * @return If successful, contains a populated UniquePtr. Otherwise, the
   *     UniquePtr will be set to nullptr.
   */
  UniquePtr<struct chreSensorThreeAxisData> convertUsfBiasUpdateToData(
      const usf::UsfSensorTransformConfigEvent *update);

  /**
   * Get the index into the calibrated sensor data array for the given sensor
   * type.
   *
   * @param usfSensorType The USF sensor type to obtain the index for.
   * @return The index into the data array for the given sensor type. If the
   *     sensor type isn't present in the array, kNumUsfCalSensors is returned.
   */
  static size_t getCalArrayIndex(const usf::UsfSensorType usfSensorType);

  //! A struct to store a sensor's calibration data.
  struct UsfCalData {
    uint64_t timestamp;
    float bias[3];
    bool hasBias;
  };

  //! The list of calibrated USF sensors supported.
  enum class UsfCalSensor : size_t {
    AccelCal,
    GyroCal,
    MagCal,
    NumCalSensors,
  };

  static constexpr size_t kNumUsfCalSensors =
      static_cast<size_t>(UsfCalSensor::NumCalSensors);

  //! Cal data of all the cal sensors.
  UsfCalData mCalData[kNumUsfCalSensors] = {};

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
