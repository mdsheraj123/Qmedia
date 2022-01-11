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

#include "audio-recorder.h"

#include "umd-logging.h"

#define LOG_TAG "AudioRecorder"

const uint32_t AUDIO_BUFFERS_COUNT = 4;

AudioRecorder::AudioRecorder(std::string audiodev,
                             AudioRecorderConfig config,
                             IAudioRecorderCallback *callback)
  : mAudioDev(audiodev),
    mConfig(config),
    mPcm(nullptr),
    mMixer(nullptr),
    mThread(nullptr),
    mRecording(false),
    mBufSize(0),
    mCallback(callback),
    mAudioStream(nullptr),
    mErrorCode(0) {}

AudioRecorder::~AudioRecorder() {
  Stop();
}

int32_t AudioRecorder::Start() {
  const std::lock_guard<std::mutex> lock(mMutex);

  mErrorCode = 0;

  if (mCallback == nullptr) {
    UMD_LOG_ERROR ("Invalid audio callback!\n");
    return -EINVAL;
  }

  struct pcm_config cfg{};
  cfg.period_size = mConfig.period_size;
  cfg.period_count = mConfig.period_count;
  cfg.channels = mConfig.channels;
  cfg.rate = mConfig.samplerate;

  cfg.format = AudioRecorderToPcmFormat(mConfig.format);
  if (cfg.format == PCM_FORMAT_INVALID) {
    UMD_LOG_ERROR ("Unsupported audio format: %d\n", mConfig.format);
    return -EINVAL;
  }

  cfg.start_threshold = 0;
  cfg.stop_threshold = 0;
  cfg.silence_threshold = 0;

  unsigned int pcm_card, pcm_dev;
  if (mAudioDev[0] != 'h' ||
      mAudioDev[1] != 'w' ||
      mAudioDev[2] != ':' ||
      mAudioDev.length() < 4) {
    UMD_LOG_ERROR ("Invalid device name %s\n", mAudioDev.c_str());
    return -EINVAL;
  }

  if (sscanf(&mAudioDev[3], "%u,%u", &pcm_card, &pcm_dev) != 2) {
    UMD_LOG_ERROR ("Invalid device name %s\n", mAudioDev.c_str());
    return -EINVAL;
  }

  mMixer = mixer_open(pcm_card);
  if (mMixer == nullptr) {
    UMD_LOG_ERROR ("Mixer device open failed!\n");
    return -ENODEV;
  }

  if (SetMixerConfiguration(mMixer) != 0) {
    UMD_LOG_ERROR ("Audio mixer configuration failed!\n");
    return -EINVAL;
  }

  mPcm = pcm_open(pcm_card, pcm_dev, PCM_IN | PCM_MONOTONIC, &cfg);
  if (mPcm == nullptr) {
    UMD_LOG_ERROR ("Pcm open failed!\n");
    return -ENODEV;
  }

  mBufSize = pcm_frames_to_bytes(mPcm, pcm_get_buffer_size(mPcm));
  if (mBufSize == 0) {
    UMD_LOG_ERROR ("Invalid audio buffer size!\n");
    return -EINVAL;
  }

  mAudioStream = std::unique_ptr<AudioStream>(
      new AudioStream(mCallback, mBufSize, AUDIO_BUFFERS_COUNT));

  if (mAudioStream == nullptr) {
    UMD_LOG_ERROR ("Audio stream creation failed!\n");
    return -ENOMEM;
  }

  int32_t res = mAudioStream->Init();
  if (res) {
    UMD_LOG_ERROR ("Audio stream init failed!\n");
    return res;
  }

  if (mThread == nullptr) {
    mRecording = true;
    mThread = std::unique_ptr<std::thread>(
       new std::thread(&AudioRecorder::AudioThreadHandler, this));
  }

  if (mThread == nullptr) {
    UMD_LOG_ERROR ("Audio thread creation failed!\n");
    return -ENOMEM;
  }

  return 0;
}

int32_t AudioRecorder::Stop() {
  const std::lock_guard<std::mutex> lock(mMutex);

  mRecording = false;

  if (mThread != nullptr) {
    mThread->join();
    mThread = nullptr;
  }

  if (mMixer != nullptr) {
    if (MixerRelease(mMixer) != 0) {
      UMD_LOG_ERROR ("Audio mixer release failed!\n");
    }
    mixer_close(mMixer);
    mMixer = nullptr;
  }

  if (mPcm != nullptr) {
    pcm_close(mPcm);
    mPcm = nullptr;
  }

  mAudioStream = nullptr;

  return mErrorCode;
}

void AudioRecorder::AudioThreadHandler() {
  while (mRecording) {
    AudioBuffer *buffer;
    int32_t res = mAudioStream->GetBuffer(&buffer);
    if (res) {
      UMD_LOG_ERROR ("Audio stream get buffer failed.\n");
      mErrorCode = res;
      break;
    }

    if (buffer == nullptr) {
      UMD_LOG_ERROR ("Invalid audio stream buffer.\n");
      mErrorCode = -ENOMEM;
      break;
    }

    res = pcm_read(mPcm, buffer->data, mBufSize);
    if (!res) {
      struct timespec ts;
      unsigned int avail = 0;
      if (pcm_get_htimestamp(mPcm, &avail, &ts)) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
      }
      buffer->timestamp = ts.tv_sec * 1000000000LL + ts.tv_nsec;
      buffer->size = mBufSize;
      mAudioStream->SubmitBuffer(buffer);
    } else {
      UMD_LOG_ERROR ("pcm_read fail: %s\n", pcm_get_error(mPcm));
      mAudioStream->ReturnBuffer(buffer);
      mErrorCode = res;
      break;
    }
  }
}

pcm_format AudioRecorder::AudioRecorderToPcmFormat(AudioFormat format) {
  switch (format) {
    case AUDIO_FORMAT_S8:
      return PCM_FORMAT_S8;
      break;
    case AUDIO_FORMAT_S16_LE:
      return PCM_FORMAT_S16_LE;
      break;
    case AUDIO_FORMAT_S24_LE:
      return PCM_FORMAT_S24_LE;
      break;
    case AUDIO_FORMAT_S24_3LE:
      return PCM_FORMAT_S24_3LE;
      break;
    case AUDIO_FORMAT_S32_LE:
      return PCM_FORMAT_S32_LE;
      break;
    default:
      UMD_LOG_ERROR ("Unsupported audio format!\n");
      return PCM_FORMAT_INVALID;
      break;
  }
}

int32_t AudioRecorder::SetMixerConfiguration(struct mixer *mixer) {
  struct mixer_ctl *ctl = nullptr;
  int32_t ret = 0;

  ctl = mixer_get_ctl_by_name(mixer, "TX_CDC_DMA_TX_3 Channels");
  if (ctl == nullptr) {
    return -ENODEV;
  }

  ret = mixer_ctl_set_enum_by_string(ctl, "One");
  if (ret) {
    return ret;
  }

  ctl = mixer_get_ctl_by_name(mixer, "TX_AIF1_CAP Mixer DEC2");
  if (ctl == nullptr) {
    return -ENODEV;
  }

  ret = mixer_ctl_set_value(ctl, 0, 1);
  if (ret) {
    return ret;
  }

  ctl = mixer_get_ctl_by_name(mixer, "TX DMIC MUX2");
  if (ctl == nullptr) {
    return -ENODEV;
  }

  ret = mixer_ctl_set_enum_by_string(ctl, "DMIC0");
  if (ret) {
    return ret;
  }

  ctl = mixer_get_ctl_by_name(mixer, "TX_DEC2 Volume");
  if (ctl == nullptr) {
    return -ENODEV;
  }

  ret = mixer_ctl_set_value(ctl, 0, 112);
  if (ret) {
    return ret;
  }

  ctl = mixer_get_ctl_by_name(mixer, "MultiMedia1 Mixer TX_CDC_DMA_TX_3");
  if (ctl == nullptr) {
    return -ENODEV;
  }

  ret = mixer_ctl_set_value(ctl, 0, 1);
  if (ret) {
    return ret;
  }

  return 0;
}

int32_t AudioRecorder::MixerRelease(struct mixer *mixer) {
  struct mixer_ctl *ctl = nullptr;
  int32_t ret = 0;

  ctl = mixer_get_ctl_by_name(mixer, "TX_AIF1_CAP Mixer DEC2");
  if (ctl == nullptr) {
    return -ENODEV;
  }

  ret = mixer_ctl_set_value(ctl, 0, 0);
  if (ret) {
    return ret;
  }

  ctl = mixer_get_ctl_by_name(mixer, "TX DMIC MUX2");
  if (ctl == nullptr) {
    return -ENODEV;
  }

  ret = mixer_ctl_set_enum_by_string(ctl, "ZERO");
  if (ret) {
    return ret;
  }

  ctl = mixer_get_ctl_by_name(mixer, "MultiMedia1 Mixer TX_CDC_DMA_TX_3");
  if (ctl == nullptr) {
    return -ENODEV;
  }

  ret = mixer_ctl_set_value(ctl, 0, 0);
  if (ret) {
    return ret;
  }

  return 0;
}