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

#include "umd-camera.h"
#include "umd-logging.h"

#define LOG_TAG "UmdCamera"

const uint32_t STREAM_BUFFER_COUNT = 4;
const uint32_t UAC_SAMPLE_RATE = 48000;
const uint32_t AUDIO_RECORDER_PERIOD_SIZE = 1024;
const uint32_t AUDIO_RECORDER_PERIOD_COUNT = 4;
const uint32_t AUDIO_RECORDER_NUM_CHANNELS = 2;
const uint32_t VIDEO_BUFFER_TIMEOUT = 1000; // [ms]

UmdCamera::UmdCamera(std::string uvcdev, std::string uacdev, std::string micdev, int cameraId)
  : mGadget(nullptr),
    mVsetup({}),
    mUmdVideoCallbacks({
        UmdCamera::setupVideoStream,
        UmdCamera::enableVideoStream,
        UmdCamera::disableVideoStream,
        UmdCamera::handleVideoControl}),
    mUvcDev(uvcdev),
    mUacDev(uacdev),
    mMicDev(micdev),
    mCameraId(cameraId),
    mStreamId(-1),
    mRequestId(-1),
    mDeviceClient(nullptr),
    mAllocDeviceInterface(nullptr),
    mClientCb({}),
    mLastFrameNumber(-1),
    mVideoBufferQueue(VIDEO_BUFFER_TIMEOUT),
    mAudioRecorder(nullptr) {}

UmdCamera::~UmdCamera() {
  mActive = false;

  mMsg.push(UmdCameraMessage::CAMERA_TERMINATE);

  mCameraThread->join();

  mAudioRecorder->Stop();

  if (mGadget != nullptr)
    umd_gadget_free (mGadget);

  if (nullptr != mAllocDeviceInterface) {
    AllocDeviceFactory::DestroyAllocDevice(mAllocDeviceInterface);
  }
}

int32_t UmdCamera::Initialize() {

  mClientCb.errorCb = [&](
      CameraErrorCode errorCode,
      const CaptureResultExtras &extras) { ErrorCb(errorCode, extras); };

  mClientCb.idleCb = [&]() { IdleCb(); };

  mClientCb.peparedCb = [&](int id) { PreparedCb(id); };

  mClientCb.shutterCb = [&](const CaptureResultExtras &extras,
                            int64_t ts) { ShutterCb(extras, ts); };

  mClientCb.resultCb = [&](const CaptureResult &result) { ResultCb(result); };

  mAllocDeviceInterface = AllocDeviceFactory::CreateAllocDevice();
  if (nullptr == mAllocDeviceInterface) {
    UMD_LOG_ERROR ("Alloc device creation failed!\n");
    return -ENODEV;
  }

  mDeviceClient = new Camera3DeviceClient(mClientCb);
  if (nullptr == mDeviceClient.get()) {
    UMD_LOG_ERROR ("Invalid camera device client!\n");
    return -ENOMEM;
  }

  auto ret = mDeviceClient->Initialize();
  if (ret) {
    UMD_LOG_ERROR ("Camera client initialization failed!\n");
    return ret;
  }

  ret = mDeviceClient->OpenCamera(mCameraId);
  if (ret) {
    UMD_LOG_ERROR ("Camera %d open failed!\n", mCameraId);
    return ret;
  }

  ret = mDeviceClient->GetCameraInfo(mCameraId, &mStaticInfo);
  if (ret) {
    UMD_LOG_ERROR ("GetCameraInfo failed!\n");
    return ret;
  }

  ret = mDeviceClient->CreateDefaultRequest(RequestTemplate::PREVIEW,
                                            &mRequest.metadata);
  if (ret) {
    UMD_LOG_ERROR ("Camera CreateDefaultRequest failed!\n");
    return ret;
  }

  mGadget = umd_gadget_new (mUvcDev.c_str(), mUacDev.c_str(), &mUmdVideoCallbacks, this);
  if (nullptr == mGadget) {
    UMD_LOG_ERROR ("Failed to create UMD gadget!\n");
    return -ENODEV;
  }

  AudioRecorderConfig config;
  config.samplerate = UAC_SAMPLE_RATE;
  config.format = AUDIO_FORMAT_S16_LE;
  config.period_size = AUDIO_RECORDER_PERIOD_SIZE;
  config.period_count = AUDIO_RECORDER_PERIOD_COUNT;
  config.channels = AUDIO_RECORDER_NUM_CHANNELS;
  mAudioRecorder = std::unique_ptr<AudioRecorder>(
      new AudioRecorder(mMicDev, config, this));

  if (mAudioRecorder == nullptr) {
    UMD_LOG_ERROR ("AudioRecorder creation failed!\n");
    return -ENOMEM;
  }

  mCameraThread = std::unique_ptr<std::thread>(
      new std::thread(&UmdCamera::cameraThreadHandler, this));

  if (nullptr == mCameraThread) {
    UMD_LOG_ERROR ("Camera thread creation failed!\n");
    return -ENOMEM;
  }

  return 0;
}

bool UmdCamera::setupVideoStream(UmdVideoSetup * stmsetup, void * userdata) {
  UmdCamera *ctx = static_cast<UmdCamera*>(userdata);

  const std::lock_guard<std::mutex> lock(ctx->mCameraMutex);

  UMD_LOG_INFO ("Stream setup: %ux%u@%.2f - %c%c%c%c\n", stmsetup->width,
      stmsetup->height, stmsetup->fps, UMD_FMT_NAME (stmsetup->format));

  ctx->mVsetup = *stmsetup;
  return true;
}

bool UmdCamera::enableVideoStream(void * userdata) {
  UMD_LOG_DEBUG ("Stream ON\n");
  UmdCamera *ctx = static_cast<UmdCamera*>(userdata);

  ctx->mMsg.push(UmdCameraMessage::CAMERA_START);
  ctx->mActive = true;
  return true;
}

bool UmdCamera::disableVideoStream(void * userdata) {
  UMD_LOG_DEBUG ("Stream Off\n");
  UmdCamera *ctx = static_cast<UmdCamera*>(userdata);

  ctx->mMsg.push(UmdCameraMessage::CAMERA_STOP);
  ctx->mActive = false;
  return true;
}

void UmdCamera::SetExposureCompensation (CameraMetadata & meta, int32_t compensation)
{
  meta.update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &compensation, 1);
}

void UmdCamera::GetExposureCompensation (CameraMetadata & meta, int16_t * compensation)
{
  if (meta.exists(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION)) {
    *compensation = static_cast<int16_t>(
        meta.find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION).data.i32[0]);
  }
}

void UmdCamera::SetExposureTime (CameraMetadata & meta, uint32_t exposure)
{
  int64_t exposure_time = exposure * 100000;
  meta.update(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time, 1);
}

void UmdCamera::GetExposureTime (CameraMetadata & meta, uint32_t * exposure)
{
  if (meta.exists(ANDROID_SENSOR_EXPOSURE_TIME)) {
    int64_t exposure_time = meta.find(ANDROID_SENSOR_EXPOSURE_TIME).data.i64[0];
    *exposure = exposure_time / 100000;
  }
}

void UmdCamera::SetExposureMode (CameraMetadata & meta, uint8_t mode)
{
  uint8_t exposure_mode;
  switch (mode) {
    case UMD_VIDEO_EXPOSURE_MODE_AUTO:
      exposure_mode = ANDROID_CONTROL_AE_MODE_ON;
      break;
    case UMD_VIDEO_EXPOSURE_MODE_SHUTTER:
      exposure_mode = ANDROID_CONTROL_AE_MODE_OFF;
      break;
    default:
      UMD_LOG_ERROR ("Unsupported Exposure mode: %d!\n", mode);
      return;
  }
  meta.update(ANDROID_CONTROL_AE_MODE, &exposure_mode, 1);
}

void UmdCamera::GetExposureMode (CameraMetadata & meta, uint8_t * mode)
{
  if (meta.exists(ANDROID_CONTROL_AE_MODE)) {
    uint8_t ae_mode = meta.find(ANDROID_CONTROL_AE_MODE).data.u8[0];
    if (ae_mode == ANDROID_CONTROL_AE_MODE_ON) {
      *mode = UMD_VIDEO_EXPOSURE_MODE_AUTO;
    } else if (ae_mode == ANDROID_CONTROL_AE_MODE_OFF) {
      *mode = UMD_VIDEO_EXPOSURE_MODE_SHUTTER;
    }
  }
}

bool UmdCamera::handleVideoControl(uint32_t id, uint32_t request,
                                   void * payload, void * userdata) {
  UmdCamera *ctx = static_cast<UmdCamera*>(userdata);
  CameraMetadata metadata;
  if (!ctx->GetCameraMetadata(metadata)) {
    return false;
  }

  UMD_LOG_INFO ("Control: 0x%X, Request: 0x%X\n", id, request);

  switch (id) {
    case UMD_VIDEO_CTRL_BRIGHTNESS:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetExposureCompensation (metadata, *((int16_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetExposureCompensation (metadata, (int16_t*) payload);
          break;
        default:
          UMD_LOG_ERROR ("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_CONTRAST:
      break;
    case UMD_VIDEO_CTRL_SATURATION:
      break;
    case UMD_VIDEO_CTRL_SHARPNESS:
      break;
    case UMD_VIDEO_CTRL_BACKLIGHT_COMPENSATION:
      break;
    case UMD_VIDEO_CTRL_ANTIBANDING:
      break;
    case UMD_VIDEO_CTRL_WB_TEMPERTURE:
      break;
    case UMD_VIDEO_CTRL_WB_MODE:
      break;
    case UMD_VIDEO_CTRL_EXPOSURE_TIME:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetExposureTime (metadata, *((uint32_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetExposureTime (metadata, (uint32_t*) payload);
          break;
        default:
          UMD_LOG_ERROR ("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_EXPOSURE_MODE:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetExposureMode (metadata, *((uint8_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetExposureMode (metadata, (uint8_t*) payload);
          break;
        default:
          UMD_LOG_ERROR ("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_EXPOSURE_PRIORITY:
      break;
    case UMD_VIDEO_CTRL_FOCUS_MODE:
      break;
    case UMD_VIDEO_CTRL_ZOOM:
    case UMD_VIDEO_CTRL_PANTILT:

    default:
      UMD_LOG_ERROR ("Unknown control request 0x%X!\n", id);
      break;
  }
  if (request == UMD_CTRL_SET_REQUEST) {
    if (!ctx->SetCameraMetadata(metadata)) {
      UMD_LOG_ERROR ("Set camera metadata failed!\n");
      return false;
    }
  }
  return true;
}

void UmdCamera::ErrorCb(CameraErrorCode errorCode,
                           const CaptureResultExtras &extras) {
  UMD_LOG_ERROR("%s: ErrorCode: %d frameNumber %d requestId %d\n", __func__,
      errorCode, extras.frameNumber, extras.requestId);
}

void UmdCamera::IdleCb() {
  UMD_LOG_DEBUG("%s: Idle state notification\n", __func__);
}

void UmdCamera::ShutterCb(const CaptureResultExtras &, int64_t) {
  UMD_LOG_DEBUG("%s \n", __func__);
}

void UmdCamera::PreparedCb(int stream_id) {
  UMD_LOG_DEBUG("%s: Stream with id: %d prepared\n", __func__, stream_id);
}

void UmdCamera::ResultCb(const CaptureResult &result) {
  UMD_LOG_DEBUG("%s: Result requestId: %d partial count: %d\n", __func__,
      result.resultExtras.requestId, result.resultExtras.partialResultCount);
}

void UmdCamera::StreamCb(StreamBuffer buffer) {
  int maxsize = 0;
  int size = 0;
  uint8_t *mapped_buffer = nullptr;
  MemAllocFlags usage;
  MemAllocError ret;

  switch (buffer.info.format) {
    case BufferFormat::kBLOB:
      maxsize = buffer.info.plane_info[0].stride;
      size = buffer.info.plane_info[0].width * buffer.info.plane_info[0].height / 2;
      break;
    case BufferFormat::kYUY2:
      size = buffer.info.plane_info[0].stride * buffer.info.plane_info[0].height * 2;
      break;
    default:
      UMD_LOG_ERROR ("Unsupported format %d!\n", buffer.info.format);
      goto fail_return;
      break;
  }

  if (mActive) {
    usage.flags = IMemAllocUsage::kSwReadOften;
    ret = mAllocDeviceInterface->MapBuffer(
                                     buffer.handle, 0,
                                     0, buffer.info.plane_info[0].width,
                                     buffer.info.plane_info[0].height,
                                     usage, (void **)&mapped_buffer);

    if ((MemAllocError::kAllocOk != ret) || (NULL == mapped_buffer)) {
      UMD_LOG_ERROR("%s: Unable to map buffer: %p res: %d\n", __func__,
          mapped_buffer, ret);
      goto fail_return;
    }

    uint32_t bufidx = umd_gadget_submit_buffer (mGadget, UMD_VIDEO_STREAM_ID,
        mapped_buffer, size, maxsize, buffer.timestamp);
    if (bufidx < 0) {
      goto fail_unmap;
    }
    mVideoBufferQueue.push(std::make_pair (buffer, bufidx));
    return;
  }

fail_unmap:
  mAllocDeviceInterface->UnmapBuffer(buffer.handle);

fail_return:
  mDeviceClient->ReturnStreamBuffer(buffer);
}

void UmdCamera::onAudioBuffer(AudioBuffer *buffer) {
  if (mActive) {
    uint32_t bufidx = umd_gadget_submit_buffer (mGadget, UMD_AUDIO_STREAM_ID,
        buffer->data, buffer->size, buffer->size, buffer->timestamp);
    umd_gadget_wait_buffer (mGadget, UMD_AUDIO_STREAM_ID, bufidx);
  }
}

void UmdCamera::cameraThreadHandler() {
  bool running = true;
  while (running) {
    UmdCameraMessage event;
    mMsg.pop(event);
    switch(event) {
      case UmdCameraMessage::CAMERA_START:
        UMD_LOG_DEBUG ("CAMERA_START\n");
        if (CameraStart()) {
          UMD_LOG_ERROR ("Camera start failed.\n");
        }
        break;
      case UmdCameraMessage::CAMERA_STOP:
        UMD_LOG_DEBUG ("CAMERA_STOP\n");
        if (CameraStop()) {
          UMD_LOG_ERROR ("Camera stop failed.\n");
        }
        break;
      case UmdCameraMessage::CAMERA_SUBMIT_REQUEST:
        UMD_LOG_DEBUG ("CAMERA_SUBMIT_REQUEST\n");
        if (CameraSubmitRequest()) {
          UMD_LOG_ERROR ("Camera stop failed.\n");
        }
        break;
      case UmdCameraMessage::CAMERA_TERMINATE:
        UMD_LOG_DEBUG ("CAMERA_TERMINATE\n");
        CameraStop();
        running = false;
        break;
      default:
        UMD_LOG_ERROR("Unknown event type: %d", event);
    }
  }
}

void UmdCamera::videoBufferLoop() {
  while (mActive || mVideoBufferQueue.size()) {
    std::pair<StreamBuffer, int32_t> buffer_pair;
    if (!mVideoBufferQueue.pop(buffer_pair)) {
      StreamBuffer buffer = buffer_pair.first;
      int32_t bufidx = buffer_pair.second;
      umd_gadget_wait_buffer (mGadget, UMD_VIDEO_STREAM_ID, bufidx);

      if (buffer.handle == nullptr) {
        UMD_LOG_ERROR("Invalid buffer handle\n");
        continue;
      }

      mAllocDeviceInterface->UnmapBuffer(buffer.handle);
      mDeviceClient->ReturnStreamBuffer(buffer);
    } else {
      UMD_LOG_ERROR("Video buffer timeout!\n");
    }
  }

  UMD_LOG_INFO("videoBufferLoop terminate!\n");
}

bool UmdCamera::CameraStart() {
  UMD_LOG_DEBUG ("Camera start\n");

  const std::lock_guard<std::mutex> lock(mCameraMutex);

  mVideoBufferThread = std::unique_ptr<std::thread>(
      new std::thread(&UmdCamera::videoBufferLoop, this));

  if (nullptr == mVideoBufferThread) {
    UMD_LOG_ERROR ("Video buffer thread creation failed!\n");
    return -ENOMEM;
  }

  if (mVsetup.width == 0 || mVsetup.height == 0) {
    UMD_LOG_ERROR ("Invalid stream resolution: %dx%d!\n",
        mVsetup.width, mVsetup.height == 0);
    return false;
  }

  auto ret = mDeviceClient->BeginConfigure();
  if (0 != ret) {
    UMD_LOG_ERROR ("Camera BeginConfigure failed!\n");
    return false;
  }

  mStreamParams = {};
  mStreamParams.bufferCount = STREAM_BUFFER_COUNT;

  switch (mVsetup.format) {
    case UMD_VIDEO_FMT_YUYV:
      mStreamParams.format = PixelFormat::YCBCR_422_I;
      break;
    case UMD_VIDEO_FMT_MJPEG:
      mStreamParams.format = PixelFormat::BLOB;
      break;
    default:
      UMD_LOG_ERROR ("Unsupported video format: %d!\n", mVsetup.format);
      return false;
      break;
  }

  mStreamParams.width = mVsetup.width;
  mStreamParams.height = mVsetup.height;
  mStreamParams.allocFlags.flags = IMemAllocUsage::kSwReadOften;
  mStreamParams.cb = [&](StreamBuffer buffer) { StreamCb(buffer); };

  mStreamId = mDeviceClient->CreateStream(mStreamParams);
  if (mStreamId < 0) {
    UMD_LOG_ERROR("Camera CreateStream failed!\n");
    return false;
  }

  mRequest.streamIds.add(mStreamId);

  ret = mDeviceClient->EndConfigure();
  if (0 != ret) {
    UMD_LOG_ERROR ("Camera EndConfigure failed!\n");
    return false;
  }

  if (!CameraSubmitRequestLocked()) {
    UMD_LOG_ERROR ("SubmitRequest failed!\n");
    return false;
  }

  if(mAudioRecorder->Start()) {
    UMD_LOG_ERROR ("Failed to start audio recorder!\n");
  }

  return true;
}

bool UmdCamera::CameraStop() {
  UMD_LOG_DEBUG ("Camera stop\n");

  const std::lock_guard<std::mutex> lock(mCameraMutex);

  if (mAudioRecorder->Stop()) {
    UMD_LOG_ERROR ("Audio recorder failed!\n");
  }

  if (mVideoBufferThread == nullptr) {
    UMD_LOG_ERROR ("Video loop thread not started!\n");
    return -EINVAL;
  }

  auto ret = mDeviceClient->CancelRequest(mRequestId, &mLastFrameNumber);
  if (0 != ret) {
    UMD_LOG_ERROR ("Camera CancelRequest failed!\n");
  }

  UMD_LOG_INFO("%s: Preview request cancelled last frame number: %" PRId64 "\n",
      __func__, mLastFrameNumber);

  ret = mDeviceClient->WaitUntilIdle();
  if (0 != ret) {
    UMD_LOG_ERROR ("Camera WaitUntilIdle failed!\n");
  }

  mVideoBufferThread->join();

  for (uint32_t i = 0; i < mRequest.streamIds.size(); i++) {
    ret = mDeviceClient->DeleteStream(mRequest.streamIds[i], false);
    if (0 != ret) {
      UMD_LOG_ERROR ("Camera DeleteStream failed!\n");
    }
  }
  mRequest.streamIds.clear();
  mStreamId = -1;

  return true;
}

bool UmdCamera::CameraSubmitRequest() {
  UMD_LOG_DEBUG ("Camera submit request\n");

  const std::lock_guard<std::mutex> lock(mCameraMutex);

  return CameraSubmitRequestLocked();
}

bool UmdCamera::CameraSubmitRequestLocked() {
  mRequestId = mDeviceClient->SubmitRequest(mRequest, true, &mLastFrameNumber);
  if (0 > mRequestId) {
    UMD_LOG_ERROR ("Camera SubmitRequest failed!\n");
    return false;
  }
  return true;
}

bool UmdCamera::GetCameraMetadata(CameraMetadata &meta) {
  const std::lock_guard<std::mutex> lock(mCameraMutex);
  meta.clear();
  meta.append(mRequest.metadata);
  return true;
}

bool UmdCamera::SetCameraMetadata(CameraMetadata &meta) {
  const std::lock_guard<std::mutex> lock(mCameraMutex);
  mRequest.metadata.clear();
  mRequest.metadata.append(meta);
  mMsg.push(UmdCameraMessage::CAMERA_SUBMIT_REQUEST);
  return true;
}