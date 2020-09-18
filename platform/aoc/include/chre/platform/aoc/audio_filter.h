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

#ifndef CHRE_PLATFORM_AOC_AUDIO_FILTER_H_
#define CHRE_PLATFORM_AOC_AUDIO_FILTER_H_

#include "chre_api/chre/audio.h"
#include "efw/include/filter.h"
#include "filters/audio/common/include/audio_metadata.h"

namespace chre {

/**
 * This class implements an audio filter, whose primary purpose is to
 * read audio samples from a shared ring buffer, process it, and route
 * it to registered audio clients. 'Filter', in this context, means a
 * data processing unit (audio data itself is raw, with no actual filtering
 * done on it). The audio filter is fully controlled by an instance of an
 * audio controller object (see audio_controller.h). The interaction occurs
 * as follows:
 *  - The controller instantiates a filter, and upon notification from the
 *    remote core that a ring buffer is available, binds it to the filter.
 *  - The controller also binds an IPC Pipe to the filter's input. The pipe's
 *    primary (and only) purpose is to notify the filter that there's fresh
 *    data in the ring buffer.
 *  - The filter then reads the ring buffer, does some processing (strip
 *    timestamps, format data, etc.), and sends said data out to clients via
 *    the CHRE AudioRequestManager.
 *  The filter instantiates a worker thread, which is woken up by any one of
 *  the following two mechanisms (outside of the automatic watchdog pet wakeup):
 *  - 'Signal': A signal is an internal (to the core) command, sent by the main
 *  audio controller, to notify the filter of local (to the core) conditions.
 *  For example, CHRE called a free data event, which the controller receives,
 *  and notifies the filter. Signals are processed by overloading the
 *  SignalProcessor method of the filter base class.
 *  - 'Input': An input is a cross core notification that the filter registers
 *  to, via the pipe that's bound to it. On a pipe notification, the thread
 *  wakes up to read/process data, which is done by overloading the
 *  InputProcessor method of the filter base class.
 *
 *  TODO:
 *  1 - Audio samples are currently collected and processed in SRAM, need to
 *      copy out to DRAM before passing to nanoapps, to free up the 2-sec
 *      buffer to resume collecting samples.
 *  2 - Only a single channel (of the 4 available) is currently supported
 *      (blocked by 1)..
 *  3 - The Input processor drops audio samples while the current buffer
 *      is being serviced by clients (blocked by 1)
 */

class AudioFilter : public Filter {
 public:
  // Defines the index of a signal sent by the controller
  // on receiving CHRE's freeAudioDataEvent.
  static constexpr size_t kBufferReleasedSignalIndex = 0;

  // Defines the index at which the audio controller's IPC pipe binds to
  // the filter input. Note that this is dictated by the 'Num Inputs'
  // parameter to the filter constructor.
  static constexpr size_t kPipeIndex = 0;

  // Defines the index at which the audio controller's IPC ring buffer
  // binds to the filter's ring buffer array. Note that this is dictated
  // by the 'Num Ring Buffers' parameter to the filter constructor.
  static constexpr size_t kRingIndex = 0;

  static constexpr uint32_t kSupportedSampleRate = 16000;
  static constexpr uint32_t kSupportedDurationSeconds = 2;

  AudioFilter();

  /**
   * Callback that's called at the start of  the 'Start' method,
   * and before the worker thread has been instantiated.
   *
   * @return 0 on success, -1 otherwise.
   */
  int StartCallback() override final;

  /**
   * Callback that gets called at the end of the 'Stop' method,
   * and after the worker thread has been shutdown.
   *
   * @return true on success
   */
  bool StopCallback() override final;

  /**
   * Method that handles commands that are internal to the core,
   * typically sent by the audio controller. The argument to the
   * function is predicated by the 'Command Depth' argument to the
   * constructor. If the filter is expected to process a new signal,
   * then the constructor argument needs to be incremented as well,
   * as any index exceeding this is automatically dropped.
   *
   * @param index Signal ID (less than command depth) sent by the
   * controller
   *
   * @return true on valid command and processing success
   */
  bool SignalProcessor(int index) override final;

  /**
   * Method that handles cross core pipe notifications, if one was bound
   * to the filter's input. The 'pin' argument to the function is predicated
   * by the 'Num Filter Inputs' argument to the constructor. If the filter is
   * expected to process a new input (via a new pipe being bound to its
   * input), then the constructor argument needs to be incremented as well,
   * as any index exceeding this is automatically dropped. The pipe index
   * depends on the order that InputBind was called. No lookup is provided,
   * and the application is expected to keep track of this (maybe via an
   * external enum).
   *
   * @param pin Index of the input pipe that was bound to the filter.
   *
   * @param message Input pipe notification to be processed. This can be
   * a nullptr - which just means that the pipe wanted to notify a certain
   * condition, but had no metadata to provide.
   *
   * @param size Input message size. Can be zero, if the pipe had no
   * metadata to send.
   */
  void InputProcessor(int pin, void *message, size_t size) override final;

  /**
   * Callback that's invoked on CHRE's release audio data event.
   *
   * TODO: This needs to be refactored when we store audio samples in DRAM.
   */
  void OnBufferReleased();

 private:
  static constexpr size_t kSamplesPer2Sec =
      kSupportedSampleRate * kSupportedDurationSeconds;

  int16_t mSampleBufferSram[kSamplesPer2Sec];
  int16_t *mSampleBufferDram = nullptr;
  void *mRingBufferHandle = nullptr;
  size_t mSampleCount = 0;
  bool mDramBufferInUse = false;

  struct chreAudioDataEvent mDataEvent = {
      .version = CHRE_AUDIO_DATA_EVENT_VERSION,
      .reserved = {0, 0, 0},
      .handle = 0,
      .timestamp = 0,
      .sampleRate = kSupportedSampleRate,
      .sampleCount = 0,
      .format = CHRE_AUDIO_DATA_FORMAT_16_BIT_SIGNED_PCM,
      .samplesS16 = nullptr};
};

}  // namespace chre

#endif  // CHRE_PLATFORM_AOC_AUDIO_FILTER_H_
