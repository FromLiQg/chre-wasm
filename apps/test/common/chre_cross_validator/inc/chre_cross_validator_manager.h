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

#ifndef CHRE_CROSS_VALIDATOR_MANAGER_H_
#define CHRE_CROSS_VALIDATOR_MANAGER_H_

#include <chre.h>
#include <pb_encode.h>

#include "chre_cross_validation.nanopb.h"

#include "chre/util/optional.h"
#include "chre/util/singleton.h"

namespace chre {

namespace cross_validator {

// TODO(b/154271551): Break up the Manager class into more fine-grained classes
// to avoid it becoming to complex.

/**
 * Class to manage a CHRE cross validator nanoapp.
 */
class Manager {
 public:
  /**
   * Calls the cleanup helper method. This dtor is called on a singleton deinit
   * call.
   */
  ~Manager();

  /**
   * Handle a CHRE event.
   *
   * @param senderInstanceId The instand ID that sent the event.
   * @param eventType The type of the event.
   * @param eventData The data for the event.
   */
  void handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                   const void *eventData);

 private:
  /**
   * The enum that describes the type of cross validator in use.
   */
  enum class CrossValidatorType { SENSOR };

  /**
   * Struct to hold the state of the cross validator nanoapp.
   */
  struct CrossValidatorState {
    // Set upon received start message and read when nanoapp ends to handle
    // cleanup
    CrossValidatorType crossValidatorType;
    // Set when start message received and checked against when a sensor data
    // event from CHRE comes in
    uint8_t sensorType;
    // Set when start message is received and default sensor is found for
    // requested sensor type and read when the sensor configuration is being
    // cleaned up. Unused in non-sensor type validations
    uint32_t sensorHandle;
    // The host endpoint which is read from the start message and used when
    // sending data back to AP.
    uint64_t timeStart;
    // The host endpoint which is read from the start message and used when
    // sending data back to AP.
    uint16_t hostEndpoint = CHRE_HOST_ENDPOINT_BROADCAST;
    // The sensor for this validation uses continuous reporting mode.
    bool isContinuous;

    CrossValidatorState(CrossValidatorType crossValidatorTypeIn,
                        uint8_t sensorTypeIn, uint32_t sensorHandleIn,
                        uint64_t timeStartIn, uint16_t hostEndpointIn,
                        bool isContinuousIn)
        : crossValidatorType(crossValidatorTypeIn),
          sensorType(sensorTypeIn),
          sensorHandle(sensorHandleIn),
          timeStart(timeStartIn),
          hostEndpoint(hostEndpointIn),
          isContinuous(isContinuousIn) {}
  };

  //! The current state of the nanoapp.
  //! Unset if start message was not received or error while processing start
  //! message.
  chre::Optional<CrossValidatorState> mCrossValidatorState;

  /**
   * Make a SensorDatapoint proto message.
   *
   * @param sampleDataFromChre The sample data from CHRE.
   * @param currentTimestamp The current CHRE timestamp.
   *
   * @return The SensorDatapoint proto message.
   */
  static chre_cross_validation_SensorDatapoint makeDatapoint(
      bool (*encodeFunc)(pb_ostream_t *, const pb_field_t *, void *const *),
      const void *sampleDataFromChre, uint64_t currentTimestamp);

  /**
   * Encodes the values array of a sensor datapoint proto message. Used as the
   * encode field of the SenorDatapoint message.
   *
   * @param stream The stream to write to.
   * @param field The field to write to (unused).
   * @param arg The data passed in order to write to the stream.
   * @return true if successful.
   */
  static bool encodeThreeAxisSensorDatapointValues(pb_ostream_t *stream,
                                                   const pb_field_t * /*field*/,
                                                   void *const *arg);

  /**
   * Encodes the datapoints into a SensorData message.
   *
   * @param stream The stream to write to.
   * @param field The field to write to (unused).
   * @param arg The data passed in order to write to the stream.
   * @return true if successful.
   */
  static bool encodeThreeAxisSensorDatapoints(pb_ostream_t *stream,
                                              const pb_field_t * /*field*/,
                                              void *const *arg);

  /**
   * Encodes the datapoints into a SensorData message.
   *
   * @param stream The stream to write to.
   * @param field The field to write to (unused).
   * @param arg The data passed in order to write to the stream.
   * @return true if successful.
   */
  static bool encodeFloatSensorDatapoints(pb_ostream_t *stream,
                                          const pb_field_t * /*field*/,
                                          void *const *arg);

  /**
   * Encodes the datapoints into a SensorData message.
   *
   * @param stream The stream to write to.
   * @param field The field to write to (unused).
   * @param arg The data passed in order to write to the stream.
   * @return true if successful.
   */
  static bool encodeProximitySensorDatapoints(pb_ostream_t *stream,
                                              const pb_field_t * /*field*/,
                                              void *const *arg);

  /**
   * Encodes a single float value into values list of SensorDatapoint object.
   *
   * @param stream The stream to write to.
   * @param field The field to write to (unused).
   * @param arg The data passed in order to write to the stream.
   * @return true if successful.
   */
  static bool encodeFloatSensorDatapointValue(pb_ostream_t *stream,
                                              const pb_field_t * /*field*/,
                                              void *const *arg);

  /**
   * Encodes a single promximity value into values list of SensorDatapoint
   * object, converting the isNear property into 5.0 (false) or 0.0 (true) in
   * the process.
   *
   * @param stream The stream to write to.
   * @param field The field to write to (unused).
   * @param arg The data passed in order to write to the stream.
   * @return true if successful.
   */
  static bool encodeProximitySensorDatapointValue(pb_ostream_t *stream,
                                                  const pb_field_t * /*field*/,
                                                  void *const *arg);

  /**
   * Handle a start sensor message.
   *
   * @param startSensorCommand The StartSensorCommand proto message received.
   *
   * @return true if successful.
   */
  bool handleStartSensorMessage(
      const chre_cross_validation_StartSensorCommand &startSensorCommand);

  /**
   * @param header The sensor data header from CHRE sensor data event.
   *
   * @return true if header is valid.
   */
  bool isValidHeader(const chreSensorDataHeader &header);

  /**
   * Handle a start message from CHRE with the given data from host.
   *
   * @param hostData The data from host that has a start message.
   */
  void handleStartMessage(const chreMessageFromHostData *hostData);

  /**
   * Handle a message from the host.
   *
   * @param senderInstanceId The instance ID of the sender of this message.
   * @param hostData The data from the host message.
   */
  void handleMessageFromHost(uint32_t senderInstanceId,
                             const chreMessageFromHostData *hostData);

  /**
   * @param threeAxisDataFromChre Three axis sensor data from CHRE.
   * @param sensorType The sensor type that sent the three axis data.
   *
   * @return The Data proto message that is ready to be sent to host with three
   * axis data.
   */
  chre_cross_validation_Data makeSensorThreeAxisData(
      const chreSensorThreeAxisData *threeAxisDataFromChre, uint8_t sensorType);

  /**
   * @param floatDataFromChre Float sensor data from CHRE.
   * @param sensorType The sensor type that sent the three axis data.
   *
   * @return The Data proto message that is ready to be sent to host with float
   * data.
   */
  chre_cross_validation_Data makeSensorFloatData(
      const chreSensorFloatData *floatDataFromChre, uint8_t sensorType);

  /**
   * @param proximtyDataFromChre Proximity sensor data from CHRE.
   * @param sensorType The sensor type that sent the three axis data.
   *
   * @return The Data proto message that is ready to be sent to host with float
   * data.
   */
  chre_cross_validation_Data makeSensorProximityData(
      const chreSensorByteData *proximityDataFromChre);

  /**
   * Handle sensor three axis data from CHRE.
   *
   * @param threeAxisDataFromChre The data from CHRE to parse.
   * @param sensorType The sensor type that sent the three axis data.
   */
  void handleSensorThreeAxisData(
      const chreSensorThreeAxisData *threeAxisDataFromChre, uint8_t sensorType);

  /**
   * Handle sensor float data from CHRE.
   *
   * @param floatDataFromChre The data from CHRE to parse.
   * @param sensorType The sensor type that sent the float data.
   */
  void handleSensorFloatData(const chreSensorFloatData *floatDataFromChre,
                             uint8_t sensorType);

  /**
   * Handle proximity sensor data from CHRE.
   *
   * @param proximityDataFromChre The data to parse.
   */
  void handleProximityData(const chreSensorByteData *proximityDataFromChre);

  /**
   * Encode and send data to be validated to host.
   *
   * @param data The data to encode and send.
   */
  void encodeAndSendDataToHost(const chre_cross_validation_Data &data);

  /**
   * Determine if nanoapp is ready to process new sensor data.
   *
   * @param header The sensor data header that was received with data.
   * @param sensorType The sensor type of received data.
   *
   * @return true if state is inline with receiving this new sensor data.
   */
  bool processSensorData(const chreSensorDataHeader &header,
                         uint8_t sensorType);

  /**
   * @param sensorType The sensor type received from a CHRE sensor data event.
   *
   * @return true if sensorType matches the sensorType of current cross
   * validator state.
   */
  bool sensorTypeIsValid(uint8_t sensorType);

  /**
   * Cleanup the manager by tearing down any CHRE API resources that were used
   * during validation.
   */
  void cleanup();
};

// The chre cross validator manager singleton.
typedef chre::Singleton<Manager> ManagerSingleton;

}  // namespace cross_validator

}  // namespace chre

#endif  // CHRE_CROSS_VALIDATOR_MANAGER_H_
