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

#include "chre/platform/aoc/audio_controller.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/assert.h"

#include "aoc/ff1/audio/include/controller_audio_input.h"  // For command IDs
#include "aoc/include/processor_aoc.h"
#include "efw/include/environment_ipc.h"
#include "efw/include/ring_buffer_ipc.h"

namespace chre {

namespace {

// Helper function to get the IPC Array parameter for the conroller
// constructor. The function exists because of a current limitation in
// the EFW Controller class constructor which requires an IPC array
// variable to be passed in.
inline IPCommunication **GetIpcArray() {
  static IPCommunication *arr =
      EnvironmentIpc::Instance()->IpcByID(IPC_CHANNEL_CMD_CHRE_AUDIO_NOTIF_ID);
  return &arr;
}

}  // namespace

AudioController::AudioController()
    : Controller(
          "CHRE_CTR" /* name */, EFWObject::Root(),
          tskIDLE_PRIORITY + 5 /* priority */, 1024 /* stack size in words */,
          GetIpcArray() /* ipc array */, 1 /*ipc descriptor array size */,
          5 /* max outstanding ipc notifications */,
          10 /* max total notifications */, 1 /* num IPC ring buffers */, 10),
      mControllerIpcAoc("CHRE_CTR_IPC" /* name */, EFWObject::Root(),
                        tskIDLE_PRIORITY + 5 /* priority */,
                        ProcessorAoC::FF1 /* remote core */,
                        EnvironmentIpc::Instance()->IpcByID(
                            IPC_CHANNEL_CMD_CHRE_AUDIO_INPUT_ID),
                        1 /* notifDepth */, 1 /* NumIpcRingBuffers */),
      mPipeIpcReceiverAoc(
          "CHRE_PIPE" /* name */,
          EnvironmentIpc::Instance()->IpcByID(IPC_CHANNEL_DTA_CHRE_AUDIO_ID),
          ProcessorAoC::A32 /* Primary core */, EFWObject::Root()) {
  LOGV("MainController created");

  TaskSpawn();
}

void AudioController::SetUp() {
  Controller::SetUp();
  mControllerIpcAoc.Start();
}

void AudioController::SetUpRemoteCore() {
  if (!mDspCoreInitDone) {
    int rc = -1;
    struct CMD_HDR cmd;
    // See controller_audio_input.h in EFW for command ID definitions.
    uint16_t cmdId = ControllerAudioInput::CMD_AUDIO_INPUT_CHRE_SETUP_ID;
    if ((rc = mControllerIpcAoc.CmdRelay(&cmd, cmdId, sizeof(CMD_HDR),
                                         portMAX_DELAY)) != 0) {
      LOGE("Failed to send setup cmd to DSP rc: %d", rc);
    } else {
      mDspCoreInitDone = true;
    }
  }
}

void AudioController::TearDownRemoteCore() {
  if (mDspCoreInitDone) {
    int rc = -1;
    struct CMD_HDR cmd;
    // See controller_audio_input.h in EFW for command ID definitions.
    uint16_t cmdId = ControllerAudioInput::CMD_AUDIO_INPUT_CHRE_TEARDOWN_ID;
    if ((rc = mControllerIpcAoc.CmdRelay(&cmd, cmdId, sizeof(CMD_HDR),
                                         portMAX_DELAY)) != 0) {
      LOGE("Failed to send teardown cmd to DSP rc: %d", rc);
    }
    mDspCoreInitDone = false;
  }
}

bool AudioController::StopCallback() {
  mFilter.Stop();
  mFilter.RingUnbind(AudioFilter::kRingIndex);

  ring_buffer_ipc_[AudioFilter::kRingIndex]->WriteDescrReturn();
  TearDownRemoteCore();

  mControllerIpcAoc.Stop();

  return true;
}

void AudioController::TearDown() {
  Controller::TearDown();

  mControllerIpcAoc.Stop();
  mFilter.Stop();
  mFilter.RingUnbind(AudioFilter::kRingIndex);
}

bool AudioController::CmdProcessor(struct CMD_HDR *cmd) {
  switch (cmd->id) {
    case ControllerAudioInput::CMD_AUDIO_INPUT_CHRE_F1_CTR_READY_ID:
      cmd->reply = OnDspReady();
      break;

    default:
      LOGD("Unknown cmd for CHRE controller");
      cmd->reply = -1;
      break;
  }

  return true;
}

int AudioController::OnDspReady() {
  int rc = -1;

  mDspCoreInitDone = true;

  if ((rc = mFilter.InputBind(AudioFilter::kPipeIndex, &mPipeIpcReceiverAoc)) !=
      0) {
    LOGE("Failed to bind pipe to filter input");
  } else if ((rc = mFilter.RingBind(
                  AudioFilter::kRingIndex,
                  ring_buffer_ipc_[AudioFilter::kRingIndex])) != 0) {
    LOGE("Failed to bind ipc ring buffer to filter");
  } else if ((rc = mFilter.Start()) != 0) {
    LOGE("Failed to start audio filter");
  } else {
    EventLoopManagerSingleton::get()
        ->getAudioRequestManager()
        .handleAudioAvailability(0 /*handle*/, true /*available*/);
  }

  return rc;
}

const chreAudioSource *AudioController::GetSource(uint32_t /*handle*/) const {
  return &kAudioSource;
}

void AudioController::SetEnabled(uint32_t /*handle*/, bool enabled) {
  mSourceStatus.enabled = enabled;
}

bool AudioController::UpdateStream(uint32_t /*handle*/, bool start) {
  bool success = false;
  // TODO: handle and other params
  if (mDspCoreInitDone) {
    if (mRequestStarted != start) {
      mRequestStarted = start;
      CMD_HDR cmd;
      cmd.reply = -1;  // negative value to test
      // see controller_audio_input.h in EFW for command ID definitions.
      uint16_t cmdId =
          start ? ControllerAudioInput::CMD_AUDIO_INPUT_MIC_CHRE_START_ID
                : ControllerAudioInput::CMD_AUDIO_INPUT_MIC_CHRE_STOP_ID;
      int rc = mControllerIpcAoc.CmdRelay(&cmd, cmdId, sizeof(CMD_HDR),
                                          portMAX_DELAY);
      LOGD("request audio send: %d", rc);
      success = (rc == 0);
    } else {
      // streaming already in the correct state for this handle
      success = true;
    }
  }

  return success;
}

bool AudioController::RequestAudio(uint32_t handle) {
  return UpdateStream(handle, true /*start*/);
}

bool AudioController::ReleaseAudio(uint32_t handle) {
  return UpdateStream(handle, false /*start*/);
}

void AudioController::OnBufferReleased() {
  mFilter.Signal(AudioFilter::kBufferReleasedSignalIndex);
}

}  // namespace chre
