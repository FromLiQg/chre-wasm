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

// TODO(b/146052784): Break up the Manager class into more fine-grained classes
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

    CrossValidatorState(CrossValidatorType crossValidatorTypeIn,
                        uint32_t sensorHandleIn, uint64_t timeStartIn,
                        uint16_t hostEndpointIn)
        : crossValidatorType(crossValidatorTypeIn),
          sensorHandle(sensorHandleIn),
          timeStart(timeStartIn),
          hostEndpoint(hostEndpointIn) {}
  };

  //! The current state of the nanoapp.
  //! Unset if start message was not received or error while processing start
  //! message.
  chre::Optional<CrossValidatorState> mCrossValidatorState;

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
   * Make a SensorDatapoint proto message.
   *
   * @param sampleDataFromChre The sample data from CHRE.
   * @param currentTimestamp The current CHRE timestamp.
   *
   * @return The SensorDatapoint proto message.
   */
  static chre_cross_validation_SensorDatapoint makeDatapoint(
      const chreSensorThreeAxisData::chreSensorThreeAxisSampleData
          &sampleDataFromChre,
      uint64_t currentTimestamp);

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
   *
   * @return The Data proto message that is ready to be sent to host.
   */
  chre_cross_validation_Data makeAccelSensorData(
      const chreSensorThreeAxisData *threeAxisDataFromChre);

  /**
   * Handle sensor three axis data from CHRE.
   *
   * @param threeAxisDataFromChre The data from CHRE to parse.
   */
  void handleSensorThreeAxisData(
      const chreSensorThreeAxisData *threeAxisDataFromChre);

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
