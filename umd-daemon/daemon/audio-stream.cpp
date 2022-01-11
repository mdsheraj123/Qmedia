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

#include "audio-stream.h"

#include "message_queue.h"
#include "umd-logging.h"

#define LOG_TAG "AudioStream"

AudioStream::AudioStream(IAudioRecorderCallback *callback, uint32_t buffer_size, uint32_t buffers_count)
  : mCallback(callback),
    mBufferSize(buffer_size),
    mBuffersCount(buffers_count),
    mThread(nullptr) {}

AudioStream::~AudioStream() {
  AudioCallbackMsg msg{};
  msg.type = AUDIO_CALLBACK_MSG_END;
  mMsg.push(msg);

  if (mThread) {
    mThread->join();
  }

  FreeBuffers();
}

int32_t AudioStream::Init() {
  for (int i = 0; i < mBuffersCount; i++) {
    AudioBuffer buffer{};
    buffer.data = new uint8_t[mBufferSize];
    if (buffer.data == nullptr) {
      UMD_LOG_ERROR ("Audio buffer allocation failed.\n");
      goto fail;
    }
    buffer.size = mBuffersCount;
    mBuffers.push_back(buffer);
  }

  if (mThread == nullptr) {
    mThread = std::unique_ptr<std::thread>(
        new std::thread(&AudioStream::StreamLoopHandler, this));
    if (mThread == nullptr) {
      UMD_LOG_ERROR ("Stream thread creation failed.\n");
      goto fail;
    }
  }

  return 0;
fail:
  FreeBuffers();
  return -ENOMEM;
}

int32_t AudioStream::GetBuffer(AudioBuffer **buffer) {
  mMutex.lock();
  while (mBuffersMap.size() == mBuffersCount) {
    mMutex.unlock();
    std::unique_lock<std::mutex> lock(mConditionMutex);
    mCondition.wait(lock);
    mMutex.lock();
  }

  for (int i = 0; i < mBuffersCount; i++) {
    AudioBuffer *buf = &mBuffers[i];
    if (mBuffersMap.find(buf) == mBuffersMap.end()) {
      mBuffersMap[buf] = i;
      *buffer = buf;
      mMutex.unlock();
      return 0;
    }
  }
  *buffer = nullptr;
  mMutex.unlock();
  return -ENOMEM;
}

void AudioStream::ReturnBuffer(AudioBuffer *buffer) {
  AudioCallbackMsg msg{};
  msg.buffer = buffer;
  msg.type = AUDIO_CALLBACK_MSG_RETURN;
  mMsg.push(msg);
}

void AudioStream::SubmitBuffer(AudioBuffer *buffer) {
  AudioCallbackMsg msg{};
  msg.buffer = buffer;
  msg.type = AUDIO_CALLBACK_MSG_SUBMIT;
  mMsg.push(msg);
}

void AudioStream::FreeBuffers() {
  for (AudioBuffer &buffer : mBuffers) {
    if (buffer.data) {
      delete[] buffer.data;
    }
  }
}

void AudioStream::StreamLoopHandler() {
  bool running = true;
  while (running || mMsg.size()) {
    AudioCallbackMsg msg{};
    mMsg.pop(msg);
    switch(msg.type) {
      case AUDIO_CALLBACK_MSG_SUBMIT:
        mCallback->onAudioBuffer(msg.buffer);
        mMutex.lock();
        if (mBuffersMap.find(msg.buffer) != mBuffersMap.end()) {
          mBuffersMap.erase(msg.buffer);
        } else {
          UMD_LOG_ERROR ("Buffer is not in buffer map : %p\n", msg.buffer);
        }
        mMutex.unlock();
        mCondition.notify_all();
        break;
      case AUDIO_CALLBACK_MSG_RETURN:
        mMutex.lock();
        if (mBuffersMap.find(msg.buffer) != mBuffersMap.end()) {
          mBuffersMap.erase(msg.buffer);
        } else {
          UMD_LOG_ERROR ("Buffer is not in buffer map : %p\n", msg.buffer);
        }
        mMutex.unlock();
        mCondition.notify_all();
        break;
      case AUDIO_CALLBACK_MSG_END:
        running = false;
        break;
      default:
        UMD_LOG_ERROR ("Invalid callback message: %d\n", msg.type);
    }
  }
}