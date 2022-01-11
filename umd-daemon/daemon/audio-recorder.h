/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <string.h>
#include <tinyalsa/asoundlib.h>

#include "audio-stream.h"
#include "audio-recorder-interface.h"

#include <thread>
#include <atomic>
#include <memory>

using namespace android;

class AudioRecorder : public IAudioRecorder {
public:
  AudioRecorder(std::string audiodev,
                AudioRecorderConfig config,
                IAudioRecorderCallback *callback);
  ~AudioRecorder();

  int32_t Start() override;
  int32_t Stop() override;

private:
  void AudioThreadHandler();
  pcm_format AudioRecorderToPcmFormat(AudioFormat format);
  int32_t SetMixerConfiguration(struct mixer *mixer);
  int32_t MixerRelease(struct mixer *mixer);

  std::string mAudioDev;
  AudioRecorderConfig mConfig;
  struct pcm *mPcm;
  struct mixer *mMixer;
  std::unique_ptr<std::thread> mThread;
  std::atomic<bool> mRecording;
  size_t mBufSize;
  IAudioRecorderCallback *mCallback;
  std::unique_ptr<AudioStream> mAudioStream;
  std::mutex mMutex;
  int32_t mErrorCode;
};