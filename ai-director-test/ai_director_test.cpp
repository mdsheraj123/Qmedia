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

#include "ai_director_test.h"
#include "umd-logging.h"

#include <fstream>

#define LOG_TAG "AIDirectorTest"

const uint32_t STREAM_BUFFER_COUNT = 4;
const uint32_t VIDEO_BUFFER_TIMEOUT = 1000; // [ms]

AIDirectorTest::AIDirectorTest(UsecaseSetup usetup, int cameraId)
  : mCameraId(cameraId),
    mDeviceClient(nullptr),
    mAllocDeviceInterface(nullptr),
    mRequestId(-1),
    mClientCb({}),
    mLastFrameNumber(-1),
    mProcessVideoQueue(VIDEO_BUFFER_TIMEOUT),
    mTransformVideoQueue(VIDEO_BUFFER_TIMEOUT),
    mUsecaseSetup(usetup),
    mOutputBuffer({}) {}

AIDirectorTest::~AIDirectorTest() {
  mActive = false;

  if (nullptr != mAllocDeviceInterface) {
    AllocDeviceFactory::DestroyAllocDevice(mAllocDeviceInterface);
  }

  ai_ctrl_deinit();
}

int32_t AIDirectorTest::Initialize() {

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

  ai_ctrl_init_t init_params;
  init_params.features_type = FEATURE_AUTOFRAMING;
  init_params.full_fov_width = mUsecaseSetup.transform_width;
  init_params.full_fov_height = mUsecaseSetup.transform_height;
  init_params.process_format = AI_CTRL_FORMAT_NV12;
  init_params.process_width = mUsecaseSetup.process_width;
  init_params.process_height = mUsecaseSetup.process_height;

  ai_status_t res;
  res = ai_ctrl_init(&init_params,
                     &AIControlRoiCallback,
                     this);

  if (res != STATUS_OK) {
    UMD_LOG_ERROR ("Unable to initialize AI director. res: %d\n", res);
    return -ENODEV;
  }

  return 0;
}

void AIDirectorTest::ErrorCb(CameraErrorCode errorCode,
                           const CaptureResultExtras &extras) {
  UMD_LOG_ERROR("%s: ErrorCode: %d frameNumber %d requestId %d\n", __func__,
      errorCode, extras.frameNumber, extras.requestId);
}

void AIDirectorTest::IdleCb() {
  UMD_LOG_DEBUG("%s: Idle state notification\n", __func__);
}

void AIDirectorTest::ShutterCb(const CaptureResultExtras &, int64_t) {
  UMD_LOG_DEBUG("%s \n", __func__);
}

void AIDirectorTest::PreparedCb(int stream_id) {
  UMD_LOG_DEBUG("%s: Stream with id: %d prepared\n", __func__, stream_id);
}

void AIDirectorTest::ResultCb(const CaptureResult &result) {
  UMD_LOG_DEBUG("%s: Result requestId: %d partial count: %d\n", __func__,
      result.resultExtras.requestId, result.resultExtras.partialResultCount);
}

void AIDirectorTest::ProcessStreamCb(StreamBuffer buffer) {
  mProcessVideoQueue.push(buffer);
}

void AIDirectorTest::TransformStreamCb(StreamBuffer buffer) {
  mTransformVideoQueue.push(buffer);
}

void AIDirectorTest::processVideoLoop() {
  while (mActive || mProcessVideoQueue.size()) {
    MemAllocFlags usage;
    MemAllocError ret;
    void *vaddr = nullptr;
    StreamBuffer buffer;
    ai_ctrl_buffer_t buff;
    ai_status_t res;

    if (mProcessVideoQueue.pop(buffer)) {
      UMD_LOG_ERROR("Video buffer timeout!\n");
      continue;
    }

    usage.flags = IMemAllocUsage::kSwReadOften;
    ret = mAllocDeviceInterface->MapBuffer(
                                   buffer.handle, 0,
                                   0, buffer.info.plane_info[0].width,
                                   buffer.info.plane_info[0].height,
                                   usage, (void **)&vaddr);

    if ((MemAllocError::kAllocOk != ret) || (nullptr == vaddr)) {
      UMD_LOG_ERROR("%s: Unable to map buffer: %p res: %d\n", __func__,
          vaddr, ret);
      goto fail_return;
    }

    buff.width = buffer.info.plane_info[0].width;
    buff.height = buffer.info.plane_info[0].height;
    buff.size = buffer.info.plane_info[0].size + buffer.info.plane_info[1].size;
    buff.vaddr = vaddr;
    buff.fd = buffer.fd;
    buff.timestamp = buffer.timestamp;
    buff.format = BufferFormatToAIFormat(buffer.info.format);
    buff.num_of_planes = buffer.info.num_planes;

    for (int i = 0; i < buff.num_of_planes; i++) {
      buff.plains[i].width = buffer.info.plane_info[i].width;
      buff.plains[i].height = buffer.info.plane_info[i].height;
      buff.plains[i].stride = buffer.info.plane_info[i].stride;
      buff.plains[i].offset = buffer.info.plane_info[i].offset;
    }

    buff.roi = {0, 0, buff.width, buff.height};

    res = ai_ctrl_process(&buff);
    if (res != STATUS_OK) {
      UMD_LOG_ERROR("ai_ctrl_process failed: %d\n", ret);
    }

fail_unmap:
    mAllocDeviceInterface->UnmapBuffer(buffer.handle);
fail_return:
    mDeviceClient->ReturnStreamBuffer(buffer);
  }
  UMD_LOG_INFO("videoBufferLoop terminate!\n");
}

void AIDirectorTest::transformVideoLoop() {
  while (mActive || mTransformVideoQueue.size()) {
    MemAllocFlags usage;
    MemAllocError ret;
    void *vaddr = nullptr;
    StreamBuffer buffer;

    ai_status_t res;
    ai_ctrl_buffer_t inbuf;
    ai_ctrl_buffer_t outbuf;

    if (mTransformVideoQueue.pop(buffer)) {
      UMD_LOG_ERROR("Video buffer timeout!\n");
      continue;
    }

    usage.flags = IMemAllocUsage::kSwReadOften;
    ret = mAllocDeviceInterface->MapBuffer(
                                   buffer.handle, 0,
                                   0, buffer.info.plane_info[0].width,
                                   buffer.info.plane_info[0].height,
                                   usage, (void **)&vaddr);

    if ((MemAllocError::kAllocOk != ret) || (nullptr == vaddr)) {
      UMD_LOG_ERROR("%s: Unable to map buffer: %p res: %d\n", __func__,
          vaddr, ret);
      goto fail_return;
    }

    inbuf.width = buffer.info.plane_info[0].width;
    inbuf.height = buffer.info.plane_info[0].height;
    inbuf.size = buffer.info.plane_info[0].size + buffer.info.plane_info[1].size;
    inbuf.vaddr = vaddr;
    inbuf.fd = buffer.fd;
    inbuf.timestamp = buffer.timestamp;
    inbuf.format = BufferFormatToAIFormat(buffer.info.format);
    inbuf.num_of_planes = buffer.info.num_planes;
    inbuf.roi = {0, 0, inbuf.width, inbuf.height};

    for (int i = 0; i < inbuf.num_of_planes; i++) {
      inbuf.plains[i].width = buffer.info.plane_info[i].width;
      inbuf.plains[i].height = buffer.info.plane_info[i].height;
      inbuf.plains[i].stride = buffer.info.plane_info[i].stride;
      inbuf.plains[i].offset = buffer.info.plane_info[i].offset;
    }

    outbuf.width = mOutputBuffer.width;
    outbuf.height = mOutputBuffer.height;
    outbuf.size = mOutputBuffer.size;
    outbuf.vaddr = mOutputBuffer.data;
    outbuf.fd = mOutputBuffer.fd;
    outbuf.timestamp = buffer.timestamp;
    outbuf.format = BufferFormatToAIFormat(mOutputBuffer.format);
    outbuf.roi = {0, 0, outbuf.width, outbuf.height};

    outbuf.num_of_planes = 2;
    outbuf.plains[0].width = mOutputBuffer.width;
    outbuf.plains[0].height = mOutputBuffer.height;
    outbuf.plains[0].stride = mOutputBuffer.stride;
    outbuf.plains[0].offset = 0;

    outbuf.plains[1].width = mOutputBuffer.width;
    outbuf.plains[1].height = mOutputBuffer.height / 2;
    outbuf.plains[1].stride = mOutputBuffer.stride;
    outbuf.plains[1].offset = mOutputBuffer.stride * mOutputBuffer.height;

    res = ai_ctrl_map_buffer(inbuf.vaddr, inbuf.fd, inbuf.size);
    if (res != STATUS_OK) {
      UMD_LOG_ERROR("ai_ctrl_map_buffer failed: %d\n", res);
      goto fail_unmap;
    }

    res = ai_ctrl_map_buffer(outbuf.vaddr, outbuf.fd, outbuf.size);
    if (res != STATUS_OK) {
      UMD_LOG_ERROR("ai_ctrl_map_buffer failed: %d\n", res);
      goto fail_unmap_inbuf;
    }

    res = ai_ctrl_transform(&inbuf, 1, &outbuf);
    if (res != STATUS_OK) {
      UMD_LOG_ERROR("ai_ctrl_transform failed: %d\n", res);
    }

    ProcessOutputBuffer(&outbuf);

    res = ai_ctrl_unmap_buffer(outbuf.fd);
    if (res != STATUS_OK) {
      UMD_LOG_ERROR("outbuf unmap failed: %d\n", res);
    }
fail_unmap_inbuf:
    res = ai_ctrl_unmap_buffer(inbuf.fd);
    if (res != STATUS_OK) {
      UMD_LOG_ERROR("inbuf unmap failed: %d\n", res);
    }

fail_unmap:
    mAllocDeviceInterface->UnmapBuffer(buffer.handle);
fail_return:
    mDeviceClient->ReturnStreamBuffer(buffer);
  }
  UMD_LOG_INFO("videoBufferLoop terminate!\n");
}

bool AIDirectorTest::CameraStart() {
  UMD_LOG_DEBUG ("Camera start\n");

  const std::lock_guard<std::mutex> lock(mCameraMutex);

  if (!AllocateOutputBuffer()) {
    UMD_LOG_ERROR("Output buffer allocation failed!\n");
    return false;
  }

  mActive = true;

  mProcessThread = std::unique_ptr<std::thread>(
      new std::thread(&AIDirectorTest::processVideoLoop, this));

  if (nullptr == mProcessThread) {
    UMD_LOG_ERROR ("Video buffer thread creation failed!\n");
    return -ENOMEM;
  }

  mTransformThread = std::unique_ptr<std::thread>(
    new std::thread(&AIDirectorTest::transformVideoLoop, this));

  if (nullptr == mTransformThread) {
    UMD_LOG_ERROR ("Video buffer thread creation failed!\n");
    return -ENOMEM;
  }

  auto ret = mDeviceClient->BeginConfigure();
  if (0 != ret) {
    UMD_LOG_ERROR ("Camera BeginConfigure failed!\n");
    return false;
  }

  mStreamParams[0] = {};
  mStreamParams[0].bufferCount = STREAM_BUFFER_COUNT;
  mStreamParams[0].format = PixelFormat::YCBCR_420_888;
  mStreamParams[0].width = mUsecaseSetup.process_width;
  mStreamParams[0].height = mUsecaseSetup.process_height;
  mStreamParams[0].allocFlags.flags = IMemAllocUsage::kSwReadOften |
                                      IMemAllocUsage::kHwCameraWrite;
  mStreamParams[0].cb = [&](StreamBuffer buffer) { ProcessStreamCb(buffer); };

  mStreamId[0] = mDeviceClient->CreateStream(mStreamParams[0]);
  if (mStreamId[0] < 0) {
    UMD_LOG_ERROR("Camera CreateStream failed!\n");
    return false;
  }

  mRequest.streamIds.add(mStreamId[0]);

  mStreamParams[1] = {};
  mStreamParams[1].bufferCount = STREAM_BUFFER_COUNT;
  mStreamParams[1].format = PixelFormat::YCBCR_420_888;
  mStreamParams[1].width = mUsecaseSetup.transform_width;
  mStreamParams[1].height = mUsecaseSetup.transform_height;
  mStreamParams[1].allocFlags.flags = IMemAllocUsage::kSwReadOften |
                                      IMemAllocUsage::kHwCameraWrite;
  mStreamParams[1].cb = [&](StreamBuffer buffer) { TransformStreamCb(buffer); };

  mStreamId[1] = mDeviceClient->CreateStream(mStreamParams[1]);
  if (mStreamId[1] < 0) {
    UMD_LOG_ERROR("Camera CreateStream failed!\n");
    return false;
  }

  mRequest.streamIds.add(mStreamId[1]);

  ret = mDeviceClient->EndConfigure();
  if (0 != ret) {
    UMD_LOG_ERROR ("Camera EndConfigure failed!\n");
    return false;
  }

  if (!CameraSubmitRequestLocked()) {
    UMD_LOG_ERROR ("SubmitRequest failed!\n");
    return false;
  }

  return true;
}

bool AIDirectorTest::CameraStop() {
  UMD_LOG_DEBUG ("Camera stop\n");

  const std::lock_guard<std::mutex> lock(mCameraMutex);

  if (mProcessThread == nullptr) {
    UMD_LOG_ERROR ("Video loop thread not started!\n");
    return -EINVAL;
  }

  if (mTransformThread == nullptr) {
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

  mActive = false;
  mProcessThread->join();
  mProcessThread = nullptr;

  mTransformThread->join();
  mTransformThread = nullptr;

  for (uint32_t i = 0; i < mRequest.streamIds.size(); i++) {
    ret = mDeviceClient->DeleteStream(mRequest.streamIds[i], false);
    if (0 != ret) {
      UMD_LOG_ERROR ("Camera DeleteStream failed!\n");
    }
    mStreamId[i] = -1;
  }
  mRequest.streamIds.clear();

  if (!FreeOutputBuffer()) {
    UMD_LOG_ERROR("Output buffer free failed!\n");
    return false;
  }

  return true;
}

bool AIDirectorTest::CameraSubmitRequest() {
  UMD_LOG_DEBUG ("Camera submit request\n");

  const std::lock_guard<std::mutex> lock(mCameraMutex);

  return CameraSubmitRequestLocked();
}

bool AIDirectorTest::CameraSubmitRequestLocked() {
  mRequestId = mDeviceClient->SubmitRequest(mRequest, true, &mLastFrameNumber);
  if (0 > mRequestId) {
    UMD_LOG_ERROR ("Camera SubmitRequest failed!\n");
    return false;
  }
  return true;
}

bool AIDirectorTest::AllocateOutputBuffer() {
  UMD_LOG_INFO("%s\n", __func__);
  MemAllocFlags usage;
  usage.flags =
      IMemAllocUsage::kSwReadOften |
      IMemAllocUsage::kHwRender;

  mOutputBuffer = {};
  mOutputBuffer.width = mUsecaseSetup.output_width;
  mOutputBuffer.height = mUsecaseSetup.output_height;

  MemAllocError ret = mAllocDeviceInterface->AllocBuffer(
      mOutputBuffer.handle,
      mOutputBuffer.width,
      mOutputBuffer.height,
      static_cast<int32_t>(PixelFormat::YCBCR_420_888),
      usage,
      &mOutputBuffer.stride);

  mOutputBuffer.format = BufferFormat::kNV12;

  mOutputBuffer.size =  3 * mOutputBuffer.height *  mOutputBuffer.stride / 2;

  if (MemAllocError::kAllocOk != ret) {
    mOutputBuffer.handle = nullptr;
    return false;
  }

  mOutputBuffer.fd = mOutputBuffer.handle->GetFD();

  ret = mAllocDeviceInterface->MapBuffer(
      mOutputBuffer.handle,
      0, 0,
      mOutputBuffer.width,
      mOutputBuffer.height,
      usage, (void **)&mOutputBuffer.data);

  if (MemAllocError::kAllocOk != ret) {
    mAllocDeviceInterface->FreeBuffer(mOutputBuffer.handle);
    mOutputBuffer.handle = nullptr;
    return false;
  }
  return true;
}

bool AIDirectorTest::FreeOutputBuffer() {
  UMD_LOG_INFO("%s\n", __func__);

  if (mOutputBuffer.handle == nullptr) {
    UMD_LOG_ERROR("Invalid buffer handle!\n");
    return false;
  }

  mAllocDeviceInterface->UnmapBuffer(mOutputBuffer.handle);
  mAllocDeviceInterface->FreeBuffer(mOutputBuffer.handle);
  mOutputBuffer.handle = nullptr;

  return true;
}

ai_ctrl_format_t AIDirectorTest::BufferFormatToAIFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::kNV12:
      return AI_CTRL_FORMAT_NV12;
      break;
    case BufferFormat::kNV21:
      return AI_CTRL_FORMAT_NV21;
      break;
    default:
      return AI_CTRL_FORMAT_INVALID;
  }
}

void AIDirectorTest::ProcessOutputBuffer(ai_ctrl_buffer_t *outbuf) {
  UMD_LOG_INFO("%s\n", __func__);
  std::stringstream filename;
  std::string ext;
  static int frame_number = 0;

  if (outbuf == nullptr) {
    UMD_LOG_ERROR("Output buffer is NULL\n");
    return;
  }

  if (frame_number % 10 == 0) {
    filename << "/data/misc/media/frame_" << frame_number \
        << "_dim_" << outbuf->width << "_" << outbuf->height << ".bin";

    UMD_LOG_INFO("%s: dump frame: %s", __func__, filename.str().c_str());

    std::ofstream ofs(filename.str(), std::ios::out | std::ios::binary);
    char *data = reinterpret_cast<char*>(outbuf->vaddr);
    ofs.write(data, outbuf->size);
    ofs.close();
  }

  frame_number++;
}

void AIDirectorTest::AIControlRoiCallback(void * usr_data, ai_ctrl_roi *roi) {
  UMD_LOG_INFO("%s\n", __func__);

  if (roi != nullptr) {
    UMD_LOG_INFO("%s ROI top: %d, left:%d, bottom:%d, right:%d\n", __func__,
        roi->top, roi->left, roi->bottom, roi->right);
  }
}
