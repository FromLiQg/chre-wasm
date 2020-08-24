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

#ifndef CHRE_PLATFORM_AOC_AUDIO_CONTROLLER_H_
#define CHRE_PLATFORM_AOC_AUDIO_CONTROLLER_H_

#include "chre/platform/log.h"
#include "chre/platform/system_time.h"
#include "chre/target_platform/init.h"
#include "chre/util/time.h"
#include "chre_api/chre/audio.h"

#include "aoc/common/include/controller_ipc_aoc.h"
#include "aoc/common/include/pipe_ipc_receiver_aoc.h"

#include "efw/include/controller.h"

#include "chre/platform/aoc/audio_filter.h"

namespace chre {

/**
 * This class implements an audio controller, which sets up a filter object
 * for buffering audio (see audio_filter.h), and manages both local and
 * remote core commands to route audio data from the microphone to CHRE.
 * To ensure proper timing of operation, commands are processed in a request-
 * response pattern. The controller handles messages inbound from the remote
 * core directly, via a PipeIPCReceiver object that it instantiates and binds
 * to, while delegating outbound messages to a ControllerIPC object
 * that it also instantiates. The controller handles all CHRE platform audio
 * calls, relaying and collecting relevant information and data from the
 * audio filter.
 *
 * Limitations:
 *  - Only a single microphone channel is currently supported
 */
class AudioController : public Controller {
 public:
  static constexpr size_t kMaxAocMicrophones = 1;

  AudioController();

  /**
   * Calls the base class SetUp method, initializes audio source data,
   * and initializes cross core communication.
   */
  void SetUp() override;

  /**
   * Sends a command to the remote core indicating that the A32 Audio
   * Controller is fully up and ready to process data. This prompts
   * the DSP to setup its own CHRE Audio Controller.
   */
  void SetUpRemoteCore();

  /**
   * Calls the base class TearDown method, and unbinds the shared ring
   * buffer from the audio filter, and shuts the filter down.
   */
  void TearDown() override;

  /**
   * Handles commands inbound to the filter. This method is only
   * invoked if the IPC channel for this controller and its cross
   * core peer has been configured and setup correctly, else all
   * messages are dropped by default by the worker thread.
   *
   * @return We always return true from this function to not trigger
   * assertions in the underlying CmdThread, regardless of whether
   * we received a valid command or not. On receiving a command not
   * recognized by the controller, we simply set the command reply to
   * an error value (-1).
   */
  bool CmdProcessor(struct CMD_HDR *cmd) override;

  /**
   * Get audio source information
   *
   * @return Pointer to chreAudioSource
   */
  const chreAudioSource *GetSource(uint32_t handle) const;

  /**
   * Enables audio requests on a handle
   *
   * @param handle Handle to be enabled
   *
   * @param enabled Handle is enabled if true, disabled otherwise
   */
  void SetEnabled(uint32_t handle, bool enabled);

  /**
   * Request audio data from given handle/source.
   *
   * @param handle Handle to an audio source to request data from.
   *
   * @return true on success
   */
  bool RequestAudio(uint32_t handle);

  /**
   * Cancel/Release audio data events from given handle.
   *
   * @param handle Handle to audio source to cancel data from
   *
   * @return true on success
   */
  bool ReleaseAudio(uint32_t handle);

  /**
   * Called by the CHRE release audio data event callback, this function
   * lets the filter know that the current audio buffer was processed
   * by all clients.
   */
  void OnBufferReleased();

 private:
  static constexpr uint64_t kSupportedDurationNs =
      Seconds(AudioFilter::kSupportedDurationSeconds).toRawNanoseconds();
  static constexpr const char *kMicrophoneName = "MIC_0";

  bool mDspCoreInitDone = false;
  bool mRequestStarted = false;

  const chreAudioSource kAudioSource = {
      .name = kMicrophoneName,
      .sampleRate = AudioFilter::kSupportedSampleRate,
      .minBufferDuration = kSupportedDurationNs,
      .maxBufferDuration = kSupportedDurationNs,
      .format = CHRE_AUDIO_DATA_FORMAT_16_BIT_SIGNED_PCM};

  chreAudioSourceStatus mSourceStatus = {.enabled = false, .suspended = false};

  AudioFilter mFilter;

  // The peer Controller IPC to the CHRE controller on the DSP,
  // this class handles outbound messages from the audio controller.
  ControllerIpcAoC mControllerIpcAoc;

  // The peer Pipe Receiver to the CHRE pipe instance on the DSP,
  // this class receives IPC pipe notifications sent by the said
  // remote pipe.
  PipeIpcReceiverAoC mPipeIpcReceiverAoc;

  /**
   * Invoked by the command processor when it gets a 'Ready'
   * message from the DSP. Binds the Pipe object to the audio filter's
   * input, and binds the ipc ring buffer to the audio filter.
   *
   * @return 0 on success, -1 otherwise.
   */
  int OnDspReady();

  /**
   * Checks the current streaming state of a handle/channel, sends
   * a command to the remote core to start/stop streaming if they
   * don't match, and updates the current state on the handle.
   * Note that there's an ~50ms latency for a new 'start' on a handle.
   *
   * @param start Start streaming if enabled, stop otherwise.
   *
   * @return true on success.
   *
   * TODO: Handle the 'handle' parameter when multiple channels are supported,
   * and update the state tracking logic accordingly.
   */
  bool UpdateStream(uint32_t handle, bool start);
};

}  // namespace chre

#endif  // CHRE_PLATFORM_AOC_AUDIO_CONTROLLER_H_
