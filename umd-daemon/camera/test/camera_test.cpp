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

#include "camera_test.h"

#include <fcntl.h>
#include <fstream>

const int STREAM_BUFFER_COUNT = 4;

const char *dumpPath = "/data/misc/media";

const char *help_str = "Usage: camera_test [-c cameraId] [-w width] [-h height] [-f format]\n\n" \
                    "format:\n" \
                    "  0 - NV12\n" \
                    "  1 - JPEG\n" \
                    "  2 - YUY2\n";

void CameraTest::ErrorCb(CameraErrorCode errorCode,
                           const CaptureResultExtras &extras) {
  printf("%s: ErrorCode: %d frameNumber %d requestId %d\n", __func__,
         errorCode, extras.frameNumber, extras.requestId);
}

void CameraTest::IdleCb() {
  printf("%s: Idle state notification\n", __func__);
}

void CameraTest::ShutterCb(const CaptureResultExtras &, int64_t) {}


void CameraTest::PreparedCb(int stream_id) {
  printf("%s: Stream with id: %d prepared\n", __func__, stream_id);
}

void CameraTest::ResultCb(const CaptureResult &result) {
  printf("%s: Result requestId: %d partial count: %d\n", __func__,
         result.resultExtras.requestId, result.resultExtras.partialResultCount);
}

void CameraTest::StreamCb(StreamBuffer buffer) {
  printf("%s: streamId: %d buffer: %p ts: %" PRId64 "\n", __func__,
         buffer.stream_id, buffer.handle, buffer.timestamp);

  if(buffer.frame_number % 5 == 0) {
      uint8_t *mapped_buffer = nullptr;
      MemAllocFlags usage;
      usage.flags = IMemAllocUsage::kSwReadOften;

      auto ret = alloc_device_interface_->MapBuffer(
                            buffer.handle, 0,
                            0, buffer.info.plane_info[0].width,
                            buffer.info.plane_info[0].height,
                            usage, (void **)&mapped_buffer);

      if ((MemAllocError::kAllocOk != ret) || (NULL == mapped_buffer)) {
        printf("%s: Unable to map buffer: %p res: %d\n", __func__,
               mapped_buffer, ret);
        goto exit;
      }

      std::stringstream filename;
      std::string ext;
      int size = 0;
      switch(buffer.info.format) {
        case BufferFormat::kNV12:
          size = (buffer.info.plane_info[0].stride * buffer.info.plane_info[0].height * 3) / 2;
          ext = "nv12";
          break;
        case BufferFormat::kBLOB:
          size = buffer.info.plane_info[0].stride;
          ext = "jpeg";
          break;
        case BufferFormat::kYUY2:
          size = buffer.info.plane_info[0].stride * buffer.info.plane_info[0].height * 2;
          ext = "yuy2";
          break;
        default:
          printf("%s: Unsupported buffer format: %d\n", __func__, buffer.info.format);
          goto exit;
          break;
      }

      filename << dumpPath << "/stream_" << buffer.stream_id << "_" << buffer.frame_number << "." << ext;
      std::ofstream ofs(filename.str(), std::ios::out | std::ios::binary);
      char *data = reinterpret_cast<char*>(mapped_buffer);
      ofs.write(data, size);
      ofs.close();

      printf("%s: %s Size=%" PRIo64 " Stored\n", __func__, filename.str().c_str(), size);

      alloc_device_interface_->UnmapBuffer(buffer.handle);
  }
exit:
  device_client_->ReturnStreamBuffer(buffer);
}

CameraTest::CameraTest()
  : number_of_cameras_(0),
    last_frame_number_(0),
    stream_id_(0),
    request_id_(0) {

  client_cb_.errorCb = [&](
      CameraErrorCode errorCode,
      const CaptureResultExtras &extras) { ErrorCb(errorCode, extras); };

  client_cb_.idleCb = [&]() { IdleCb(); };

  client_cb_.peparedCb = [&](int id) { PreparedCb(id); };

  client_cb_.shutterCb = [&](const CaptureResultExtras &extras,
                            int64_t ts) { ShutterCb(extras, ts); };

  client_cb_.resultCb = [&](const CaptureResult &result) { ResultCb(result); };

  alloc_device_interface_ = AllocDeviceFactory::CreateAllocDevice();
  assert(nullptr != alloc_device_interface_.get());

  device_client_ = new Camera3DeviceClient(client_cb_);
  assert(nullptr != device_client_.get());

  auto ret = device_client_->Initialize();
  assert(0 == ret);

  number_of_cameras_ = device_client_->GetNumberOfCameras();
}

CameraTest::~CameraTest() {
  AllocDeviceFactory::DestroyAllocDevice(alloc_device_interface_);
}

bool CameraTest::StartCamera(int camera_id, int width, int height, PixelFormat format) {
  camera_id_ = camera_id;
  width_ = width;
  height_ = height;
  PixelFormat format_ = format;

  auto ret = device_client_->OpenCamera(camera_id_);
  assert(0 == ret);

  ret = device_client_->GetCameraInfo(camera_id_, &static_info_);
  assert(0 == ret);

  ret = device_client_->BeginConfigure();
  assert(0 == ret);

  memset(&stream_params_, 0, sizeof(stream_params_));
  stream_params_.bufferCount = STREAM_BUFFER_COUNT;
  stream_params_.format = format_;
  stream_params_.width = width_;
  stream_params_.height = height_;
  stream_params_.allocFlags.flags = IMemAllocUsage::kSwReadOften;
  stream_params_.cb = [&](StreamBuffer buffer) { StreamCb(buffer); };

  stream_id_ = device_client_->CreateStream(stream_params_);
  assert(stream_id_ == 0);

  preview_request_.streamIds.add(stream_id_);

  ret = device_client_->EndConfigure();
  assert(0 == ret);

  ret = device_client_->CreateDefaultRequest(RequestTemplate::PREVIEW,
                                             &preview_request_.metadata);
  assert(0 == ret);

  ret = device_client_->SubmitRequest(preview_request_, true, &last_frame_number_);
  assert(0 == ret);

  request_id_ = ret;

  return true;
}

bool CameraTest::StopCamera() {
  auto ret = device_client_->CancelRequest(request_id_, &last_frame_number_);
  assert(0 == ret);

  printf("%s: Preview request cancelled last frame number: %" PRId64 "\n",
       __func__, last_frame_number_);

  ret = device_client_->WaitUntilIdle();
  assert(0 == ret);

  return true;
}

int main(int argc, char * argv[]) {
  int32_t option = 0;
  int camera_id = 0;
  int width = 0;
  int height = 0;
  int opt_format = 0;
  PixelFormat format = PixelFormat::IMPLEMENTATION_DEFINED;

  if (argc == 1) {
    printf ("%s", help_str);
    return 0;
  }

  while ((option = getopt (argc, argv, "c:w:h:f:")) != -1) {
    switch (option) {
      case 'c':
        camera_id = std::stoi(optarg);
        break;
      case 'w':
        width = std::stoi(optarg);
        break;
      case 'h':
        height = std::stoi(optarg);
        break;
      case 'f':
        opt_format = std::stoi(optarg);
        break;
      case '?':
      default:
        printf ("%s", help_str);
        return 0;
    }
  }

  if (width == 0 || height == 0) {
    printf("Invalid resolution: %dx%d\n", width, height);
    return 0;
  }

  switch (opt_format) {
    case 0:
      format = PixelFormat::IMPLEMENTATION_DEFINED;
      break;
    case 1:
      format = PixelFormat::BLOB;
      break;
    case 2:
      format = PixelFormat::YCBCR_422_I;
      break;
    default:
      format = PixelFormat::IMPLEMENTATION_DEFINED;
  }

  sp<CameraTest> cam = new CameraTest();
  assert(0 == cam.get());

  cam->StartCamera(camera_id, width, height, format);

  pause();

  cam->StopCamera();
}
