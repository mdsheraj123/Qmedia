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

#include <VendorTagDescriptor.h>

#define LOG_TAG "UmdCamera"

using ::android::hardware::camera::common::V1_0::helper::VendorTagDescriptor;

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

  if (mCameraThread) {
    mCameraThread->join();
  }

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

uint32_t UmdCamera::GetVendorTagByName(const char * section, const char * name) {
  sp<VendorTagDescriptor> desc;
  uint32_t tag = 0;

  desc = VendorTagDescriptor::getGlobalVendorTagDescriptor();
  if (desc.get() == nullptr) {
    UMD_LOG_ERROR ("Failed to get Vendor Tag Descriptor!\n");
    return 0;
  }

  auto status = desc->lookupTag(android::String8(name),
      android::String8(section), &tag);
  if (status != 0) {
    UMD_LOG_ERROR ("Unable to find vendor tag for '%s', section '%s'!\n",
        name, section);
    return 0;
  }
  return tag;
}

bool UmdCamera::InitCameraParamsLocked() {
  CameraMetadata meta;
  if (!GetCameraMetadataLocked(meta)) {
    return false;
  }

  uint32_t tag = GetVendorTagByName (
      "org.codeaurora.qcamera3.iso_exp_priority", "use_iso_exp_priority");
  if (tag != 0) {
    int64_t isomode = 0; // ISO_MODE_AUTO
    meta.update(tag, &isomode, 1);
  }

  if (!SetCameraMetadataLocked(meta)) {
    UMD_LOG_ERROR("Set camera metadata failed!\n");
    return false;
  }

  return true;
}

void UmdCamera::SetExposureCompensation(CameraMetadata & meta, int16_t value)
{
  int32_t exposure = static_cast<int32_t>(value);
  meta.update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &exposure, 1);
}

void UmdCamera::GetExposureCompensation(CameraMetadata & meta, int16_t * value)
{
  if (meta.exists(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION)) {
    *value = static_cast<int16_t>(
        meta.find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION).data.i32[0]);
  }
}

void UmdCamera::SetContrast(CameraMetadata & meta, uint16_t value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.contrast", "level");
  if (tag != 0) {
    int32_t contrast = static_cast<int32_t>(value * 2 - 100);
    meta.update(tag, &contrast, 1);
  }
}

void UmdCamera::GetContrast(CameraMetadata & meta, uint16_t * value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.contrast", "level");
  if (tag != 0 && meta.exists(tag)) {
    int32_t contrast = meta.find(tag).data.i32[0];
    contrast = (contrast + 200) / 2;
    *value = static_cast<uint16_t>(contrast);
  }
}

void UmdCamera::SetSaturation(CameraMetadata & meta, uint16_t value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.saturation", "use_saturation");
  if (tag != 0) {
    int32_t saturation = static_cast<int32_t>(value);
    meta.update(tag, &saturation, 1);
  }
}

void UmdCamera::GetSaturation(CameraMetadata & meta, uint16_t * value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.saturation", "use_saturation");

  if (tag != 0 && meta.exists(tag)) {
    *value = static_cast<int16_t>(
        meta.find(tag).data.i32[0]);
  }
}

void UmdCamera::SetSharpness(CameraMetadata & meta, uint16_t value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.sharpness", "strength");
  if (tag != 0) {
    int32_t sharpness = static_cast<int32_t>(value);
    meta.update(tag, &sharpness, 1);
  }
}

void UmdCamera::GetSharpness(CameraMetadata & meta, uint16_t * value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.sharpness", "strength");

  if (tag != 0 && meta.exists(tag)) {
    *value = static_cast<uint16_t>(
        meta.find(tag).data.i32[0]);
  }
}

void UmdCamera::SetADRC(CameraMetadata & meta, uint16_t value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.adrc", "disable");
  if (tag != 0) {
    uint8_t adrc = static_cast<uint8_t>(value);
    meta.update(tag, &adrc, 1);
  }
}

void UmdCamera::GetADRC(CameraMetadata & meta, uint16_t * value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.adrc", "disable");

  if (tag != 0 && meta.exists(tag)) {
    *value = meta.find(tag).data.u8[0];
  }
}

void UmdCamera::SetAntibanding(CameraMetadata & meta, uint8_t value) {
  uint8_t mode = 0;
  switch (value) {
    case UMD_VIDEO_ANTIBANDING_AUTO:
      mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
      break;
    case UMD_VIDEO_ANTIBANDING_DISABLED:
      mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;
      break;
    case UMD_VIDEO_ANTIBANDING_60HZ:
      mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ;
      break;
    case UMD_VIDEO_ANTIBANDING_50HZ:
      mode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ;
      break;
    default:
      UMD_LOG_ERROR ("Unsupported Antibanding mode: %d!\n", value);
      return;
  }
  meta.update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &mode, 1);
}

void UmdCamera::GetAntibanding(CameraMetadata & meta, uint8_t * value) {
  if (meta.exists(ANDROID_CONTROL_AE_ANTIBANDING_MODE)) {
    uint8_t mode = meta.find(ANDROID_CONTROL_AE_ANTIBANDING_MODE).data.u8[0];
    switch (mode) {
      case ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO:
        *value = UMD_VIDEO_ANTIBANDING_AUTO;
        break;
      case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
        *value = UMD_VIDEO_ANTIBANDING_DISABLED;
        break;
      case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
        *value = UMD_VIDEO_ANTIBANDING_60HZ;
        break;
      case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
        *value = UMD_VIDEO_ANTIBANDING_50HZ;
        break;
      default:
        UMD_LOG_ERROR ("Unsupported Antibanding mode: %d!\n", mode);
        return;
    }
  }
}

void UmdCamera::SetISO(CameraMetadata & meta, uint16_t value) {
  int32_t priority = 0;

  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.iso_exp_priority", "select_priority");
  if (tag != 0) {
    meta.update(tag, &priority, 1);
  }

  tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.iso_exp_priority", "use_iso_value");
  if (tag != 0) {
    int32_t isovalue = value;
    meta.update(tag, &isovalue, 1);
  }
}

void UmdCamera::GetISO(CameraMetadata & meta, uint16_t * value) {
    uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.iso_exp_priority", "use_iso_value");
  if (tag != 0 && meta.exists(tag)) {
    *value = static_cast<uint16_t>(meta.find(tag).data.i32[0]);
  }
}

void UmdCamera::SetWbTemperature(CameraMetadata & meta, uint16_t value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.manualWB", "color_temperature");
   if (tag != 0) {
     int32_t color_temperature = static_cast<int32_t>(value);
     meta.update(tag, &color_temperature, 1);
   }
}

void UmdCamera::GetWbTemperature(CameraMetadata & meta, uint16_t * value) {
  uint32_t tag = GetVendorTagByName(
      "org.codeaurora.qcamera3.manualWB", "color_temperature");
   if (tag != 0 && meta.exists(tag)) {
     *value = static_cast<uint16_t>(meta.find(tag).data.i32[0]);
   }
}

void UmdCamera::SetWbMode(CameraMetadata & meta, uint8_t value) {
  int32_t mode = PARTIAL_MWB_MODE_DISABLE;
  switch (value) {
    case UMD_VIDEO_WB_MODE_AUTO:
      mode = PARTIAL_MWB_MODE_DISABLE;
      break;
    case UMD_VIDEO_WB_MODE_MANUAL:
      mode = PARTIAL_MWB_MODE_CCT;
      break;
    default:
      UMD_LOG_ERROR ("\nUnsupported WB mode: %d!\n", value);
      return;
  }

  uint32_t tag = GetVendorTagByName (
      "org.codeaurora.qcamera3.manualWB", "partial_mwb_mode");
  if (tag != 0) {
    meta.update(tag, &mode, 1);
  }
}

void UmdCamera::GetWbMode(CameraMetadata & meta, uint8_t * value) {
    uint32_t tag = GetVendorTagByName (
      "org.codeaurora.qcamera3.manualWB", "partial_mwb_mode");
  if (tag != 0 && meta.exists(tag)) {
    int32_t mode = meta.find(tag).data.i32[0];
    switch(mode) {
      case PARTIAL_MWB_MODE_DISABLE:
        *value = UMD_VIDEO_WB_MODE_AUTO;
        break;
      case PARTIAL_MWB_MODE_CCT:
        *value = UMD_VIDEO_WB_MODE_MANUAL;
        break;
      default:
        UMD_LOG_ERROR ("Unsupported WB mode: %d!\n", mode);
        break;
    }
  }
}

void UmdCamera::SetExposureTime(CameraMetadata & meta, uint32_t exposure) {
  int64_t exposure_time = exposure * 100000;
  meta.update(ANDROID_SENSOR_EXPOSURE_TIME, &exposure_time, 1);
}

void UmdCamera::GetExposureTime(CameraMetadata & meta, uint32_t * exposure)
{
  if (meta.exists(ANDROID_SENSOR_EXPOSURE_TIME)) {
    int64_t exposure_time = meta.find(ANDROID_SENSOR_EXPOSURE_TIME).data.i64[0];
    *exposure = static_cast<uint32_t>(exposure_time / 100000);
  }
}

void UmdCamera::SetExposureMode(CameraMetadata & meta, uint8_t mode) {
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

void UmdCamera::GetExposureMode(CameraMetadata & meta, uint8_t * mode)
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

void UmdCamera::SetFocusMode(CameraMetadata & meta, uint8_t value) {
  uint8_t mode  = 0;
  switch (value) {
    case UMD_VIDEO_FOCUS_MODE_AUTO:
      mode = ANDROID_CONTROL_AF_MODE_AUTO;
      break;
    case UMD_VIDEO_FOCUS_MODE_MANUAL:
      mode = ANDROID_CONTROL_AF_MODE_OFF;
      break;
    default:
      UMD_LOG_ERROR ("Unsupported Focus mode: %d!\n", value);
      return;
  }
  meta.update(ANDROID_CONTROL_AF_MODE, &mode, 1);
}

void UmdCamera::GetFocusMode(CameraMetadata & meta, uint8_t * value) {
  if (meta.exists(ANDROID_CONTROL_AF_MODE)) {
    uint8_t mode = meta.find(ANDROID_CONTROL_AF_MODE).data.u8[0];
    switch (mode) {
      case ANDROID_CONTROL_AF_MODE_AUTO:
        *value = UMD_VIDEO_FOCUS_MODE_AUTO;
        break;
      case ANDROID_CONTROL_AF_MODE_OFF:
        *value = UMD_VIDEO_FOCUS_MODE_MANUAL;
        break;
      default:
        UMD_LOG_ERROR ("Unsupported Focus mode: %d!\n", mode);
        return;
    }
  }
}

void UmdCamera::SetZoom(CameraMetadata & meta, uint16_t magnification,
    int32_t pan, int32_t tilt) {

  int32_t sensor_x = 0, sensor_y = 0, sensor_w = 0, sensor_h = 0;
  int32_t crop[4];

  if (mStaticInfo.exists(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE)) {
    sensor_x =
        mStaticInfo.find (ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE).data.i32[0];
    sensor_y =
        mStaticInfo.find (ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE).data.i32[1];
    sensor_w =
        mStaticInfo.find (ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE).data.i32[2];
    sensor_h =
        mStaticInfo.find (ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE).data.i32[3];

    int32_t zoom_w = (sensor_w - sensor_x) / (magnification / 100.0);
    int32_t zoom_h = (sensor_h - sensor_y) / (magnification / 100.0);

    int32_t zoom_x = ((sensor_w - sensor_x) - zoom_w) / 2;
    zoom_x += zoom_x * pan / (100.0 * 3600);

    int32_t zoom_y = ((sensor_h - sensor_y) - zoom_h) / 2;
    zoom_y += zoom_y * tilt / (100.0 * 3600);

    crop[0] = zoom_x;
    crop[1] = zoom_y;
    crop[2] = zoom_w;
    crop[3] = zoom_h;

    meta.update(ANDROID_SCALER_CROP_REGION, crop, 4);
  }
}

void UmdCamera::GetZoom(CameraMetadata & meta, uint16_t * magnification) {
  int32_t zoom_x = 0, zoom_y = 0, zoom_w = 0, zoom_h = 0;
  int32_t sensor_x = 0, sensor_y = 0, sensor_w = 0, sensor_h = 0;

  if (!meta.exists(ANDROID_SCALER_CROP_REGION)) {
    UMD_LOG_ERROR("Scaller crop region metadata doesn't exist.\n");
    return;
  }

  if (!mStaticInfo.exists(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE)) {
    UMD_LOG_ERROR("Sensor info active array metadata tag doesn't exist.\n");
    return;
  }

  zoom_x = meta.find(ANDROID_SCALER_CROP_REGION).data.i32[0];
  zoom_y = meta.find(ANDROID_SCALER_CROP_REGION).data.i32[1];
  zoom_w = meta.find(ANDROID_SCALER_CROP_REGION).data.i32[2];
  zoom_h = meta.find(ANDROID_SCALER_CROP_REGION).data.i32[3];

  sensor_x = mStaticInfo.find (ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE).data.i32[0];
  sensor_y = mStaticInfo.find (ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE).data.i32[1];
  sensor_w = mStaticInfo.find (ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE).data.i32[2];
  sensor_h = mStaticInfo.find (ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE).data.i32[3];

  zoom_w = (zoom_w == 0) ? sensor_w : zoom_w;
  zoom_h = (zoom_h == 0) ? sensor_h : zoom_h;

  *magnification = ((((float) sensor_w / zoom_w) +
      ((float) sensor_h / zoom_h)) / 2) * 100;
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
          ctx->SetExposureCompensation(metadata, *((int16_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetExposureCompensation(metadata, (int16_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_CONTRAST:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetContrast(metadata, *((uint16_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetContrast(metadata, (uint16_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_SATURATION:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetSaturation(metadata, *((uint16_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetSaturation(metadata, (uint16_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_SHARPNESS:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetSharpness (metadata, *((uint16_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetSharpness (metadata, (uint16_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_BACKLIGHT_COMPENSATION:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetADRC (metadata, *((uint16_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetADRC (metadata, (uint16_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_ANTIBANDING:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetAntibanding (metadata, *((uint8_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetAntibanding (metadata, (uint8_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_GAIN:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetISO (metadata, *((uint16_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetISO (metadata, (uint16_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_WB_TEMPERTURE:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetWbTemperature (metadata, *((uint16_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetWbTemperature (metadata, (uint16_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_WB_MODE:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetWbMode (metadata, *((uint8_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetWbMode (metadata, (uint8_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
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
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
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
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_EXPOSURE_PRIORITY:
      break;
    case UMD_VIDEO_CTRL_FOCUS_MODE:
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          ctx->SetFocusMode (metadata, *((uint8_t*) payload));
          break;
        case UMD_CTRL_GET_REQUEST:
          ctx->GetFocusMode (metadata, (uint8_t*) payload);
          break;
        default:
          UMD_LOG_ERROR("Unknown control request 0x%X!\n", request);
          break;
      }
      break;
    case UMD_VIDEO_CTRL_ZOOM:
    case UMD_VIDEO_CTRL_PANTILT:
      static int32_t pan = 0, tilt = 0;
      static uint16_t magnification = 100;
      switch (request) {
        case UMD_CTRL_SET_REQUEST:
          if (id == UMD_VIDEO_CTRL_ZOOM)
            magnification = *((uint16_t*) payload);

          if (id == UMD_VIDEO_CTRL_PANTILT) {
            uint8_t *data = (uint8_t*) payload;

            pan = (int32_t) data[0] | (data[1] << 8) | (data[2] << 16) |
                (data[3] << 24);
            tilt = (int32_t) data[4] | (data[5] << 8) | (data[6] << 16) |
                (data[7] << 24);
          }
          ctx->SetZoom (metadata, magnification, pan, tilt);
          break;
        case UMD_CTRL_GET_REQUEST:
            ctx->GetZoom (metadata, &magnification);

            if (id == UMD_VIDEO_CTRL_ZOOM)
              *((uint16_t*) payload) = magnification;

            if (id == UMD_VIDEO_CTRL_PANTILT) {
              uint8_t *data = (uint8_t*) payload;

              data[0] = pan & 0xFF;
              data[1] = (pan >> 8) & 0xFF;
              data[2] = (pan >> 16) & 0xFF;
              data[3] = (pan >> 24) & 0xFF;

              data[4] = tilt & 0xFF;
              data[5] = (tilt >> 8) & 0xFF;
              data[6] = (tilt >> 16) & 0xFF;
              data[7] = (tilt >> 24) & 0xFF;
           }
           break;
      }
      break;
    default:
      UMD_LOG_ERROR("Unknown control request 0x%X!\n", id);
      break;
  }
  if (request == UMD_CTRL_SET_REQUEST) {
    if (!ctx->SetCameraMetadata(metadata)) {
      UMD_LOG_ERROR("Set camera metadata failed!\n");
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

  InitCameraParamsLocked();

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
  mVideoBufferThread = nullptr;

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
  return GetCameraMetadataLocked(meta);
}

bool UmdCamera::GetCameraMetadataLocked(CameraMetadata &meta) {
  meta.clear();
  meta.append(mRequest.metadata);
  return true;
}

bool UmdCamera::SetCameraMetadata(CameraMetadata &meta) {
  const std::lock_guard<std::mutex> lock(mCameraMutex);
  return SetCameraMetadataLocked(meta);
}

bool UmdCamera::SetCameraMetadataLocked(CameraMetadata &meta) {
  mRequest.metadata.clear();
  mRequest.metadata.append(meta);
  mMsg.push(UmdCameraMessage::CAMERA_SUBMIT_REQUEST);
  return true;
}