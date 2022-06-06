/*
 *  Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <camera_device_client.h>
#include <camera_utils.h>

#include <string>
#include <thread>
#include <memory>

#include "message_queue.h"
#include "ai_ctrl.h"

using namespace ::android;
using namespace ::camera::adaptor;
using namespace ::camera;

struct HwBuffer {
  IBufferHandle handle;
  uint32_t width;
  uint32_t height;
  camera::BufferFormat format;
  uint32_t stride;
  int32_t fd;
  uint32_t size;
  void *data;
};

struct UsecaseSetup {
  uint32_t process_width;
  uint32_t process_height;
  uint32_t transform_width;
  uint32_t transform_height;
  uint32_t output_width;
  uint32_t output_height;
};

class AIDirectorTest : public RefBase {
public:
  AIDirectorTest(UsecaseSetup usetup, int cameraId);
  ~AIDirectorTest();

  int32_t Initialize();
  bool CameraStart();
  bool CameraStop();

private:
  void ErrorCb(CameraErrorCode errorCode,
                           const CaptureResultExtras &extras);
  void IdleCb();
  void ShutterCb(const CaptureResultExtras &, int64_t);
  void PreparedCb(int stream_id);
  void ResultCb(const CaptureResult &result);
  void ProcessStreamCb(StreamBuffer buffer);
  void TransformStreamCb(StreamBuffer buffer);

  void cameraThreadHandler();
  void processVideoLoop();
  void transformVideoLoop();

  bool CameraSubmitRequest();
  bool CameraSubmitRequestLocked();

  bool AllocateOutputBuffer();
  bool FreeOutputBuffer();

  ai_ctrl_format_t BufferFormatToAIFormat(BufferFormat format);
  void ProcessOutputBuffer(ai_ctrl_buffer_t *outbuf);

  static void AIControlRoiCallback(void * usr_data, ai_ctrl_roi *roi);

  int mCameraId;
  int mStreamId[2];
  bool mActive;

  sp<Camera3DeviceClient> mDeviceClient;
  IAllocDevice* mAllocDeviceInterface;
  CameraMetadata mStaticInfo;
  CameraClientCallbacks mClientCb;
  CameraStreamParameters mStreamParams[2];
  Camera3Request mRequest;

  int64_t mLastFrameNumber;
  int32_t mRequestId;

  std::mutex mCameraMutex;

  MessageQ<StreamBuffer> mProcessVideoQueue;
  MessageQ<StreamBuffer> mTransformVideoQueue;
  std::unique_ptr<std::thread> mProcessThread;
  std::unique_ptr<std::thread> mTransformThread;

  UsecaseSetup mUsecaseSetup;
  HwBuffer mOutputBuffer;
};
