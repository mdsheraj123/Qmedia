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

#include <umd-gadget.h>

#include <camera_device_client.h>
#include <camera_utils.h>

#include <string>
#include <thread>
#include <memory>

#include "message_queue.h"
#include "audio-recorder.h"

using namespace ::android;
using namespace ::camera::adaptor;
using namespace ::camera;

enum class UmdCameraMessage {
  CAMERA_START,
  CAMERA_STOP,
  CAMERA_SUBMIT_REQUEST,
  CAMERA_TERMINATE
};

class UmdCamera : public IAudioRecorderCallback, public RefBase {
public:
  UmdCamera(std::string uvcdev, std::string uacdev, std::string micdev, int cameraId);
  ~UmdCamera();

  int32_t Initialize();

private:
  static bool setupVideoStream(UmdVideoSetup * stmsetup, void * userdata);
  static bool enableVideoStream(void * userdata);
  static bool disableVideoStream(void * userdata);
  static bool handleVideoControl(uint32_t id, uint32_t request, void * payload,
                                 void * userdata);

  void SetExposureCompensation (CameraMetadata & meta, int32_t compensation);
  void GetExposureCompensation (CameraMetadata & meta, int16_t * compensation);
  void SetExposureTime (CameraMetadata & meta, uint32_t exposure);
  void GetExposureTime (CameraMetadata & meta, uint32_t * exposure);
  void SetExposureMode (CameraMetadata & meta, uint8_t mode);
  void GetExposureMode (CameraMetadata & meta, uint8_t * mode);

  void ErrorCb(CameraErrorCode errorCode,
                           const CaptureResultExtras &extras);
  void IdleCb();
  void ShutterCb(const CaptureResultExtras &, int64_t);
  void PreparedCb(int stream_id);
  void ResultCb(const CaptureResult &result);
  void StreamCb(StreamBuffer buffer);

  void onAudioBuffer(AudioBuffer * buffer) override;

  void cameraThreadHandler();
  void videoBufferLoop();

  bool CameraStart();
  bool CameraStop();
  bool CameraSubmitRequest();
  bool CameraSubmitRequestLocked();
  bool GetCameraMetadata(CameraMetadata &meta);
  bool SetCameraMetadata(CameraMetadata &meta);

  UmdGadget *mGadget;
  UmdVideoSetup mVsetup;
  UmdVideoCallbacks mUmdVideoCallbacks;
  std::mutex mGadgetMutex;
  std::string mUvcDev;
  std::string mUacDev;
  std::string mMicDev;

  std::unique_ptr<std::thread> mCameraThread;
  MessageQ<UmdCameraMessage> mMsg;

  int mCameraId;
  int mStreamId;
  bool mActive;

  sp<Camera3DeviceClient> mDeviceClient;
  IAllocDevice* mAllocDeviceInterface;
  CameraMetadata mStaticInfo;
  CameraClientCallbacks mClientCb;
  CameraStreamParameters mStreamParams;
  Camera3Request mRequest;

  int64_t mLastFrameNumber;
  int32_t mRequestId;

  std::mutex mCameraMutex;

  MessageQ<std::pair<StreamBuffer, int32_t>> mVideoBufferQueue;
  std::unique_ptr<std::thread> mVideoBufferThread;

  std::unique_ptr<IAudioRecorder> mAudioRecorder;
};
