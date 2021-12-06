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

#include <utils/RefBase.h>

#include <unordered_map>
#include <thread>
#include <memory>

#include "message_queue.h"
#include "umd-logging.h"
#include "audio-recorder-interface.h"

enum AudioCallbackMsgType {
  AUDIO_CALLBACK_MSG_NONE,
  AUDIO_CALLBACK_MSG_SUBMIT,
  AUDIO_CALLBACK_MSG_RETURN,
  AUDIO_CALLBACK_MSG_END
};

struct AudioCallbackMsg {
  AudioBuffer *buffer;
  AudioCallbackMsgType type;
};

class AudioStream : public android::RefBase {
 public:
  AudioStream(IAudioRecorderCallback *callback, uint32_t buffer_size, uint32_t buffers_count);
  ~AudioStream();

  int32_t Init();
  int32_t GetBuffer(AudioBuffer **buffer);
  void ReturnBuffer(AudioBuffer *buffer);
  void SubmitBuffer(AudioBuffer *buffer);

 private:
  void FreeBuffers();
  void StreamLoopHandler();

  IAudioRecorderCallback *mCallback;
  uint32_t mBufferSize;
  uint32_t mBuffersCount;
  std::vector<AudioBuffer> mBuffers;
  std::unordered_map <AudioBuffer*, int32_t> mBuffersMap;
  uint32_t mBufferIndex;

  std::condition_variable mCondition;
  std::mutex mConditionMutex;

  std::mutex mMutex;

  std::unique_ptr<std::thread> mThread;
  MessageQ<AudioCallbackMsg> mMsg;
};