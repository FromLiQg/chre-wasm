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
#include "chre/platform/aoc/audio_filter.h"
#include "chre/core/event_loop_manager.h"
#include "chre/platform/assert.h"
#include "chre/platform/log.h"
#include "chre/platform/system_time.h"
#include "chre/util/time.h"
#include "efw/include/ring_buffer_ipc.h"

namespace chre {

AudioFilter::AudioFilter()
    : Filter("CHRE_FLT" /* name */, EFWObject::Root() /* parent */,
             tskIDLE_PRIORITY + 5 /* priority */, 512 /* stack size words */,
             1 /* command depth */, 1 /* Num Filter inputs */,
             1 /* Num Filter Outputs */, 1 /* Num Ring Buffers */) {
  LOGV("Filter Created");

  TaskSpawn();
}

int AudioFilter::StartCallback() {
  mRingBufferHandle = ring_[kRingIndex]->ReaderRegister();
  CHRE_ASSERT(mRingBufferHandle != nullptr);
  return 0;
}

bool AudioFilter::StopCallback() {
  ring_[kRingIndex]->ReaderUnregister(mRingBufferHandle);
  mRingBufferHandle = nullptr;
  return true;
}

bool AudioFilter::SignalProcessor(int index) {
  switch (index) {
    case kBufferReleasedSignalIndex:
      OnBufferReleased();
      break;
    default:
      break;
  }
  return true;
}

void AudioFilter::InputProcessor(int /*pin*/, void *message, size_t size) {
  CHRE_ASSERT(message != nullptr);

  if (!mBufferInUse) {
    // TODO resync and synchronization, check metadata format etc.
    auto *metadata = static_cast<struct AudioInputMetadata *>(message);

    if (metadata->need_resync) {
      LOGW("Resync request received, logic not implemented yet");
    }

    constexpr size_t kBytesPerAocSample = sizeof(uint32_t);
    constexpr size_t kNumSamplesAoc = 160;
    constexpr size_t kRingSize = kNumSamplesAoc * kBytesPerAocSample *
                                 1;  // 1 channel of u32 samples per 10 ms
    uint32_t buffer[kNumSamplesAoc];
    uint32_t nBytes =
        ring_[kRingIndex]->Read(mRingBufferHandle, &buffer, kRingSize);
    CHRE_ASSERT_LOG(nBytes != 0, "Got data pipe notif, but no data in ring");
    size_t nSamples = nBytes / kBytesPerAocSample;

    if (mSampleCount == 0) {
      // TODO: Get a better estimate of the timestamp of the first sample in
      // the frame. Since the pipe notification arrives every 10ms, we could
      // subtract 10ms from now(), and maybe even keep track of the embedded
      // timestamp of the first sample of the current frame, and the last
      // sample of the previous frame.
      mDataEvent.timestamp = SystemTime::getMonotonicTime().toRawNanoseconds();
    }

    // TODO: For the initial implementation/testing, we assume that the
    // frame might not fit to a T in our current buffer, but that is the
    // expectation. With the current logic, we could end up dropping a frame,
    // revisit this to make sure we capture the exact number of samples,
    // while continuing to buffer samples either via a ping-pong scheme, or
    // a small extra scratch space.
    if ((nSamples + mSampleCount) > kSamplesPer2Sec) {
      mDataEvent.sampleCount = mSampleCount;
      mDataEvent.samplesS16 = mSampleBuffer;
      mSampleCount = 0;
      mBufferInUse = true;

      EventLoopManagerSingleton::get()
          ->getAudioRequestManager()
          .handleAudioDataEvent(&mDataEvent);

    } else {
      for (size_t i = 0; i < nSamples; ++i, ++mSampleCount) {
        // The zeroth byte of the received sample is a timestamp, which we
        // strip out.
        // TODO: Verify that there's no scaling on the remaining data, in
        // which case casting without descaling won't be the right thing to
        // do.
        mSampleBuffer[mSampleCount] =
            static_cast<int16_t>((buffer[i] >> 8) & 0xff);
      }
    }
  } else {
    LOGW(
        "Received an audio notification before the previous event was "
        "released!");
  }
}

void AudioFilter::OnBufferReleased() {
  mBufferInUse = false;
}

}  // namespace chre
