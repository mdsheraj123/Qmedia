/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 */

/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "camera_utils.h"
#include "camera_monitor.h"
#include "camera_stream.h"
#include "camera_memory_interface.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define LOG_TAG "Camera3Stream"

namespace camera {

namespace adaptor {

Camera3Stream::Camera3Stream(int id, size_t maxSize,
                             const CameraStreamParameters &outputConfiguration,
                             IAllocDevice *device,
                             Camera3Monitor &monitor)
    : mem_alloc_interface_(nullptr),
      current_buffer_stride_(0),
      id_(id),
      max_size_(maxSize),
      status_(STATUS_INTIALIZED),
      total_buffer_count_(0),
      pending_buffer_count_(0),
      hal_buffer_cnt_(0),
      client_buffer_cnt_(0),
      callbacks_(outputConfiguration.cb),
      old_usage_(),
      client_usage_(outputConfiguration.allocFlags),
      old_max_buffers_(0),
      client_max_buffers_(outputConfiguration.bufferCount),
      mem_alloc_slots_(NULL),
      hw_buffer_allocated_(0),
      monitor_(monitor),
      monitor_id_(Camera3Monitor::INVALID_ID),
      is_stream_active_(false),
      prepared_buffers_count_(0) {
  camera_stream_.id = id_;
  camera_stream_.streamType = StreamType::OUTPUT;
  camera_stream_.width = outputConfiguration.width;
  camera_stream_.height = outputConfiguration.height;
  camera_stream_.format =
    static_cast<PixelFormat>(outputConfiguration.format);
  camera_stream_.dataSpace = outputConfiguration.data_space;
  camera_stream_.rotation = outputConfiguration.rotation;
  camera_stream_.usage = static_cast<BufferUsageFlags>(
    AllocUsageFactory::GetAllocUsage().ToLocal(outputConfiguration.allocFlags));
  stream_max_buffers_ = outputConfiguration.bufferCount;

  if ((PixelFormat::BLOB == camera_stream_.format) && (0 == maxSize)) {
    CAMERA_ERROR("%s: blob with zero size\n", __func__);
    status_ = STATUS_ERROR;
  }

  if (NULL == device) {
    CAMERA_ERROR("%s:Memory allocator device is invalid!\n", __func__);
    status_ = STATUS_ERROR;
  }

  mem_alloc_interface_ = device;

  pthread_mutex_init(&lock_, NULL);
  cond_init(&output_buffer_returned_signal_);
}

Camera3Stream::~Camera3Stream() {
  if (0 <= monitor_id_) {
    monitor_.ReleaseMonitor(monitor_id_);
    monitor_id_ = Camera3Monitor::INVALID_ID;
  }

  CloseLocked();

  pthread_mutex_destroy(&lock_);
  pthread_cond_destroy(&output_buffer_returned_signal_);
  if (NULL != mem_alloc_slots_) {
    delete[] mem_alloc_slots_;
  }
}

Stream *Camera3Stream::BeginConfigure() {
  pthread_mutex_lock(&lock_);

  switch (status_) {
    case STATUS_ERROR:
      CAMERA_ERROR("%s: Error status\n", __func__);
      goto exit;
    case STATUS_INTIALIZED:
      break;
    case STATUS_CONFIG_ACTIVE:
    case STATUS_RECONFIG_ACTIVE:
      goto done;
    case STATUS_CONFIGURED:
      break;
    default:
      CAMERA_ERROR("%s: Unknown status %d", __func__, status_);
      goto exit;
  }

  camera_stream_.usage =
    AllocUsageFactory::GetAllocUsage().ToLocal(client_usage_);
  stream_max_buffers_ = client_max_buffers_;

  if (monitor_id_ != Camera3Monitor::INVALID_ID) {
    monitor_.ReleaseMonitor(monitor_id_);
    monitor_id_ = Camera3Monitor::INVALID_ID;
  }

  if (status_ == STATUS_INTIALIZED) {
    status_ = STATUS_CONFIG_ACTIVE;
  } else {
    if (status_ != STATUS_CONFIGURED) {
      CAMERA_ERROR("%s: Invalid state: 0x%x \n", __func__, status_);
      goto exit;
    }
    status_ = STATUS_RECONFIG_ACTIVE;
  }

done:
  pthread_mutex_unlock(&lock_);

  return &camera_stream_;

exit:
  pthread_mutex_unlock(&lock_);
  return NULL;
}

bool Camera3Stream::IsConfigureActive() {
  pthread_mutex_lock(&lock_);
  bool ret =
      (status_ == STATUS_CONFIG_ACTIVE) || (status_ == STATUS_RECONFIG_ACTIVE);
  pthread_mutex_unlock(&lock_);
  return ret;
}

int32_t Camera3Stream::EndConfigure() {
  int32_t res;
  pthread_mutex_lock(&lock_);
  switch (status_) {
    case STATUS_ERROR:
      CAMERA_ERROR("%s: Error status\n", __func__);
      res = -ENOSYS;
      goto exit;
    case STATUS_CONFIG_ACTIVE:
    case STATUS_RECONFIG_ACTIVE:
      break;
    case STATUS_INTIALIZED:
    case STATUS_CONFIGURED:
      CAMERA_ERROR("%s: Configuration didn't start before!\n", __func__);
      res = -ENOSYS;
      goto exit;
    default:
      CAMERA_ERROR("%s: Unknown status", __func__);
      res = -ENOSYS;
      goto exit;
  }

  monitor_id_ = monitor_.AcquireMonitor();
  if (0 > monitor_id_) {
    CAMERA_ERROR("%s: Unable to acquire monitor: %d\n", __func__, monitor_id_);
    res = monitor_id_;
    goto exit;
  }

  if (status_ == STATUS_RECONFIG_ACTIVE &&
      AllocUsageFactory::GetAllocUsage().ToCommon(static_cast<uint64_t>(camera_stream_.usage)).
        Equals(old_usage_) &&
      old_max_buffers_ == stream_max_buffers_) {
    status_ = STATUS_CONFIGURED;
    res = 0;
    goto exit;
  }

  res = ConfigureLocked();
  if (0 != res) {
    CAMERA_ERROR("%s: Unable to configure stream %d\n", __func__, id_);
    status_ = STATUS_ERROR;
    goto exit;
  }

  status_ = STATUS_CONFIGURED;
  old_usage_ =
    AllocUsageFactory::GetAllocUsage().ToCommon(static_cast<uint64_t>(camera_stream_.usage));
  old_max_buffers_ = stream_max_buffers_;

exit:
  pthread_mutex_unlock(&lock_);

  return res;
}

int32_t Camera3Stream::AbortConfigure() {
  int32_t res;
  pthread_mutex_lock(&lock_);
  switch (status_) {
    case STATUS_ERROR:
      CAMERA_ERROR("%s: Error status\n", __func__);
      res = -ENOSYS;
      goto exit;
    case STATUS_CONFIG_ACTIVE:
    case STATUS_RECONFIG_ACTIVE:
      break;
    case STATUS_INTIALIZED:
    case STATUS_CONFIGURED:
      CAMERA_ERROR("%s: Cannot abort configure that is not started\n", __func__);
      res = -ENOSYS;
      goto exit;
    default:
      CAMERA_ERROR("%s: Unknown status\n", __func__);
      res = -ENOSYS;
      goto exit;
  }

  camera_stream_.usage = static_cast<BufferUsageFlags>(
    AllocUsageFactory::GetAllocUsage().ToLocal(old_usage_));
  stream_max_buffers_ = old_max_buffers_;

  status_ = (status_ == STATUS_RECONFIG_ACTIVE) ? STATUS_CONFIGURED
                                                : STATUS_INTIALIZED;
  pthread_mutex_unlock(&lock_);
  return 0;

exit:

  pthread_mutex_unlock(&lock_);
  return res;
}

int32_t Camera3Stream::BeginPrepare() {
  int32_t res = 0;

  pthread_mutex_lock(&lock_);

  if (STATUS_CONFIGURED != status_) {
    CAMERA_ERROR("%s: Stream %d: Cannot prepare unconfigured stream with "
        "status: %d\n", __func__, id_, status_);
      res = -ENOSYS;
      goto exit;
  }

  if (is_stream_active_) {
    CAMERA_ERROR("%s: Stream %d: Cannot prepare already active stream\n",
               __func__, id_);
    res = -ENOSYS;
    goto exit;
  }

  if (0 < GetPendingBufferCountLocked()) {
    CAMERA_ERROR("%s: Stream %d: Cannot prepare stream with pending buffers\n",
               __func__, id_);
    res = -ENOSYS;
    goto exit;
  }

  prepared_buffers_count_ = 0;
  status_ = STATUS_PREPARE_ACTIVE;
  res = -ENODATA;

exit:

  pthread_mutex_unlock(&lock_);

  return res;
}

int32_t Camera3Stream::PrepareBuffer() {
  int32_t res = 0;

  pthread_mutex_lock(&lock_);

  if (STATUS_PREPARE_ACTIVE != status_) {
    CAMERA_ERROR("%s: Stream %d: Invalid status: %d\n", __func__, id_, status_);
    res = -ENOSYS;
    goto exit;
  }

  res = GetBufferLocked();
  if (0 != res) {
    CAMERA_ERROR("%s: Stream %d: Failed to pre-allocate buffer %d", __func__,
               id_, prepared_buffers_count_);
    res = -ENODEV;
    goto exit;
  }

  prepared_buffers_count_++;

  if (prepared_buffers_count_ < total_buffer_count_) {
    res = -ENODATA;
    goto exit;
  }

  res = EndPrepareLocked();

exit:

  pthread_mutex_unlock(&lock_);

  return res;
}

int32_t Camera3Stream::EndPrepare() {
  pthread_mutex_lock(&lock_);

  int32_t res = EndPrepareLocked();

  pthread_mutex_unlock(&lock_);

  return res;
}

int32_t Camera3Stream::EndPrepareLocked() {
  if (STATUS_PREPARE_ACTIVE != status_) {
    CAMERA_ERROR("%s: Stream %d: Cannot abort stream prepare with wrong"
        "status: %d\n", __func__, id_, status_);
    return -ENOSYS;
  }

  prepared_buffers_count_ = 0;
  status_ = STATUS_CONFIGURED;

  return 0;
}

bool Camera3Stream::IsPrepareActive() {
  pthread_mutex_lock(&lock_);

  bool res = (STATUS_PREPARE_ACTIVE == status_);

  pthread_mutex_unlock(&lock_);

  return res;
}

bool Camera3Stream::IsStreamActive() {
  pthread_mutex_lock(&lock_);

  bool res = is_stream_active_;

  pthread_mutex_unlock(&lock_);

  return res;
}

int32_t Camera3Stream::TearDown() {
  int32_t res = 0;

  pthread_mutex_lock(&lock_);

  if (status_ != STATUS_CONFIGURED) {
    CAMERA_ERROR(
        "%s: Stream %d: Cannot be torn down when stream"
        "is still un-configured: %d\n",
        __func__, id_, status_);
    res = -ENOSYS;
    goto exit;
  }

  if (0 < GetPendingBufferCountLocked()) {
    CAMERA_ERROR(
        "%s: Stream %d: Cannot be torn down while buffers are still pending\n",
        __func__, id_);
    res = -ENOSYS;
    goto exit;
  }

  if (0 < hw_buffer_allocated_) {
    assert(nullptr != mem_alloc_interface_);
    for (uint32_t i = 0; i < mem_alloc_buffers_.size(); i++) {
      mem_alloc_interface_->FreeBuffer(mem_alloc_buffers_.keyAt(i));
    }
    mem_alloc_buffers_.clear();
    hw_buffer_allocated_ = 0;
  }

  for (uint32_t i = 0; i < total_buffer_count_; i++) {
    mem_alloc_slots_[i] = NULL;
  }

  is_stream_active_ = false;

exit:

  pthread_mutex_unlock(&lock_);

  return res;
}

int32_t Camera3Stream::GetBuffer(::android::hardware::camera::device::V3_2::StreamBuffer *buffer,
    int64_t frame_number) {
  int32_t res = 0;

  pthread_mutex_lock(&lock_);

  if (status_ != STATUS_CONFIGURED) {
    CAMERA_ERROR(
        "%s: Stream %d: Can't retrieve buffer when stream"
        "is not configured%d\n",
        __func__, id_, status_);
    res = -ENOSYS;
    goto exit;
  }

  if (GetPendingBufferCountLocked() == total_buffer_count_) {
    CAMERA_DEBUG(
        "%s: Already retrieved maximum buffers (%d), waiting on a"
        "free one\n",
        __func__, total_buffer_count_);
    res = cond_wait_relative(&output_buffer_returned_signal_, &lock_,
                             BUFFER_WAIT_TIMEOUT);
    if (res != 0) {
      if (-ETIMEDOUT == res) {
        CAMERA_ERROR("%s: wait for output buffer return timed out\n", __func__);
        PrintBuffersInfoLocked();
      }
      goto exit;
    }
  }

  res = GetBufferLocked(buffer, frame_number);

exit:

  pthread_mutex_unlock(&lock_);
  return res;
}

int32_t Camera3Stream::PopulateMetaInfo(CameraBufferMetaData &info,
                                        IBufferHandle &handle) {
  int alignedW, alignedH;
  auto ret = mem_alloc_interface_->Perform(handle,
                                      IAllocDevice::AllocDeviceAction::GetStride,
                                      static_cast<void*>(&alignedW));

  if (MemAllocError::kAllocOk != ret) {
    CAMERA_ERROR("%s: Error in GetStrideAndHeightFromHandle() : %d\n", __func__,
               ret);
    return -EINVAL;
  }
  ret = mem_alloc_interface_->Perform(handle,
                            IAllocDevice::AllocDeviceAction::GetAlignedHeight,
                            static_cast<void*>(&alignedH));
  if (MemAllocError::kAllocOk != ret) {
    CAMERA_ERROR("%s: Error in GetStrideAndHeightFromHandle() : %d\n", __func__,
               ret);
    return -EINVAL;
  }

  CAMERA_DEBUG("%s: format(0x%x)", __func__, handle->GetFormat());

  switch (handle->GetFormat()) {
    case PixelFormat::BLOB:
      info.format = BufferFormat::kBLOB;
      info.num_planes = 1;
      info.plane_info[0].width = camera_stream_.width;
      info.plane_info[0].height = camera_stream_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = max_size_;
      info.plane_info[0].offset = 0;
      break;
    //case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
    //case HAL_PIXEL_FORMAT_NV12_ENCODEABLE:
    case PixelFormat::YCBCR_420_888:
    case PixelFormat::IMPLEMENTATION_DEFINED:
      info.format = BufferFormat::kNV12;
      info.num_planes = 2;
      info.plane_info[0].width = camera_stream_.width;
      info.plane_info[0].height = camera_stream_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      info.plane_info[1].width = camera_stream_.width;
      info.plane_info[1].height = camera_stream_.height/2;
      info.plane_info[1].stride = alignedW;
      info.plane_info[1].scanline = alignedH/2;
      info.plane_info[1].size = alignedW * (alignedH / 2);
      info.plane_info[1].offset = alignedW * alignedH;
      break;
    // case HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS_UBWC:
    //   info.format = BufferFormat::kNV12UBWC;
    //   info.num_planes = 2;
    //   info.plane_info[0].width = width;
    //   info.plane_info[0].height = height;
    //   info.plane_info[0].stride = alignedW;
    //   info.plane_info[0].scanline = alignedH;
    //   info.plane_info[0].size = MSM_MEDIA_ALIGN((alignedW * alignedH), 4096) +
    //       MSM_MEDIA_ALIGN((VENUS_Y_META_STRIDE(COLOR_FMT_NV12_UBWC, width) *
    //       VENUS_Y_META_SCANLINES(COLOR_FMT_NV12_UBWC, height)), 4096);
    //   info.plane_info[0].offset = 0;
    //   info.plane_info[1].width = width;
    //   info.plane_info[1].height = height/2;
    //   info.plane_info[1].stride = alignedW;
    //   info.plane_info[1].scanline = alignedH/2;
    //   info.plane_info[1].size = MSM_MEDIA_ALIGN((alignedW * alignedH / 2), 4096) +
    //       MSM_MEDIA_ALIGN((VENUS_UV_META_STRIDE(COLOR_FMT_NV12_UBWC, width) *
    //       VENUS_UV_META_SCANLINES(COLOR_FMT_NV12_UBWC, height)), 4096);
    //   info.plane_info[1].offset =
    //       info.plane_info[0].offset + info.plane_info[0].size;
    //   break;
    // case HAL_PIXEL_FORMAT_YCbCr_422_888:
    // case HAL_PIXEL_FORMAT_YCbCr_422_SP:
    //   info.format = BufferFormat::kNV16;
    //   info.num_planes = 2;
    //   info.plane_info[0].width = width;
    //   info.plane_info[0].height = height;
    //   info.plane_info[0].stride = alignedW;
    //   info.plane_info[0].scanline = alignedH;
    //   info.plane_info[0].size = alignedW * alignedH;
    //   info.plane_info[0].offset = 0;
    //   info.plane_info[1].width = width;
    //   info.plane_info[1].height = height;
    //   info.plane_info[1].stride = alignedW;
    //   info.plane_info[1].scanline = alignedH;
    //   info.plane_info[1].size = alignedW * alignedH;
    //   info.plane_info[1].offset = alignedW * alignedH;
    //   break;
    // case HAL_PIXEL_FORMAT_NV21_ZSL:
    //   info.format = BufferFormat::kNV21;
    //   info.num_planes = 2;
    //   info.plane_info[0].width = width;
    //   info.plane_info[0].height = height;
    //   info.plane_info[0].stride = alignedW;
    //   info.plane_info[0].scanline = alignedH;
    //   info.plane_info[0].size = alignedW * alignedH;
    //   info.plane_info[0].offset = 0;
    //   info.plane_info[1].width = width;
    //   info.plane_info[1].height = height/2;
    //   info.plane_info[1].stride = alignedW;
    //   info.plane_info[1].scanline = alignedH/2;
    //   info.plane_info[1].size = alignedW * (alignedH / 2);
    //   info.plane_info[1].offset = alignedW * alignedH;
    //   break;
    // case HAL_PIXEL_FORMAT_RAW8:
    //   info.format = BufferFormat::kRAW8;
    //   info.num_planes = 1;
    //   info.plane_info[0].width = width;
    //   info.plane_info[0].height = height;
    //   info.plane_info[0].stride = alignedW;
    //   info.plane_info[0].scanline = alignedH;
    //   info.plane_info[0].size = alignedW * alignedH;
    //   info.plane_info[0].offset = 0;
    //   break;
    case PixelFormat::RAW10:
      info.format = BufferFormat::kRAW10;
      info.num_planes = 1;
      info.plane_info[0].width = camera_stream_.width;
      info.plane_info[0].height = camera_stream_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      break;
    case PixelFormat::RAW12:
      info.format = BufferFormat::kRAW12;
      info.num_planes = 1;
      info.plane_info[0].width = camera_stream_.width;
      info.plane_info[0].height = camera_stream_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      break;
    case PixelFormat::RAW16:
      info.format = BufferFormat::kRAW16;
      info.num_planes = 1;
      info.plane_info[0].width = camera_stream_.width;
      info.plane_info[0].height = camera_stream_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      break;
    case PixelFormat::YCBCR_422_I:
      info.format = BufferFormat::kYUY2;
      info.num_planes = 1;
      info.plane_info[0].width = camera_stream_.width;
      info.plane_info[0].height = camera_stream_.height;
      info.plane_info[0].stride = alignedW;
      info.plane_info[0].scanline = alignedH;
      info.plane_info[0].size = alignedW * alignedH;
      info.plane_info[0].offset = 0;
      break;
    default:
      CAMERA_ERROR("%s: Unsupported format: %d\n", __func__,
                 handle->GetFormat());
      return -ENOENT;
  }

  return 0;
}

void Camera3Stream::ReturnBufferToClient(const ::android::hardware::camera::device::V3_2::StreamBuffer &buffer,
                                         int64_t timestamp,
                                         int64_t frame_number) {
  assert(nullptr != callbacks_);

  pthread_mutex_lock(&lock_);

  hal_buffer_cnt_--;
  client_buffer_cnt_++;

  StreamBuffer b;
  memset(&b, 0, sizeof(b));
  b.timestamp = timestamp;
  b.frame_number = frame_number;
  b.stream_id = id_;
  b.data_space = camera_stream_.dataSpace;
  b.handle = buffers_map[frame_number];
  assert(b.handle != nullptr);
  b.fd = b.handle->GetFD();
  b.size = b.handle->GetSize();
  PopulateMetaInfo(b.info, b.handle);
  is_stream_active_ = true;

  mem_alloc_interface_->Perform(
      b.handle, IAllocDevice::AllocDeviceAction::GetMetaFd,
      static_cast<void*>(&b.metafd)
  );

  pthread_mutex_unlock(&lock_);

  if (BufferStatus::OK == buffer.status) {
    callbacks_(b);
  } else {
    CAMERA_WARN("%s: Got buffer(%p) from stream(%d), frame_number(%u) and "
        " ts(%lld) with error status!", __func__, b.handle, b.stream_id,
        b.frame_number, b.timestamp);
    ReturnBuffer(b);
  }
}

int32_t Camera3Stream::ReturnBuffer(const StreamBuffer &buffer) {
  pthread_mutex_lock(&lock_);

  int32_t res = ReturnBufferLocked(buffer);
  if (res == 0) {
    pthread_cond_signal(&output_buffer_returned_signal_);
  }

  pthread_mutex_unlock(&lock_);
  return res;
}

int32_t Camera3Stream::Close() {
  pthread_mutex_lock(&lock_);
  int32_t res = CloseLocked();

  if (res == -ENOTCONN) {
    res = 0;
  }

  pthread_mutex_unlock(&lock_);
  return res;
}

int32_t Camera3Stream::ReturnBufferLocked(const StreamBuffer &buffer) {
  if (status_ == STATUS_INTIALIZED) {
    CAMERA_ERROR(
        "%s: Stream %d: Can't return buffer when we only "
        "got initialized %d\n",
        __func__, id_, status_);
    return -ENOSYS;
  }

  if (pending_buffer_count_ == 0) {
    CAMERA_ERROR("%s: Stream %d: Not expecting any buffers!\n", __func__, id_);
    return -ENOSYS;
  }

  int32_t idx = mem_alloc_buffers_.indexOfKey(buffer.handle);
  if (-ENOENT == idx) {
    CAMERA_ERROR(
        "%s: Buffer %p returned that wasn't allocated by this"
        " stream!\n",
        __func__, buffer.handle);
    return -EINVAL;
  } else {
    mem_alloc_buffers_.replaceValueFor(buffer.handle, true);
  }

  pending_buffer_count_--;
  client_buffer_cnt_--;
  CAMERA_DEBUG("%s: Stream(%d): pending_buffer_count_(%u)", __func__, id_,
      pending_buffer_count_);

  if (pending_buffer_count_ == 0 && status_ != STATUS_CONFIG_ACTIVE &&
      status_ != STATUS_RECONFIG_ACTIVE) {
    CAMERA_DEBUG("%s: Stream(%d): Changing state to idle", __func__, id_);
    monitor_.ChangeStateToIdle(monitor_id_);
  }

  return 0;
}

int32_t Camera3Stream::GetBufferLocked(::android::hardware::camera::device::V3_2::StreamBuffer *streamBuffer,
    int64_t frame_number) {
  int32_t idx = -1;
  if ((status_ != STATUS_CONFIGURED) && (status_ != STATUS_CONFIG_ACTIVE) &&
      (status_ != STATUS_RECONFIG_ACTIVE) &&
      (status_ != STATUS_PREPARE_ACTIVE)) {
    CAMERA_ERROR(
        "%s: Stream %d: Can't get buffers before being"
        " configured  or preparing %d\n",
        __func__, id_, status_);
    return -ENOSYS;
  }

  IBufferHandle handle = NULL;
  //Only pre-allocate buffers in case no valid streamBuffer
  //is passed as an argument.
  if (NULL != streamBuffer) {
    for (uint32_t i = 0; i < mem_alloc_buffers_.size(); i++) {
      if (mem_alloc_buffers_.valueAt(i)) {
        handle = mem_alloc_buffers_.keyAt(i);
        mem_alloc_buffers_.replaceValueAt(i, false);
        break;
      }
    }
  }

  if (NULL != handle) {
    for (uint32_t i = 0; i < hw_buffer_allocated_; i++) {
      if (mem_alloc_slots_[i] == handle) {
        idx = i;
        break;
      }
    }
  } else if ((NULL == handle) &&
             (hw_buffer_allocated_ < total_buffer_count_)) {
    assert(nullptr != mem_alloc_interface_);
    // Blob buffers are expected to get allocated with width equal to blob
    // max size and height equal to 1.
    int32_t buf_width, buf_height;
    if (PixelFormat::BLOB == camera_stream_.format) {
      buf_width = max_size_;
      buf_height = 1;
    } else {
      buf_width = camera_stream_.width;
      buf_height = camera_stream_.height;
    }
    MemAllocFlags memusage =
        AllocUsageFactory::GetAllocUsage().ToCommon(static_cast<uint64_t>(camera_stream_.usage));

    // Remove the CPU read/write flags set by CamX since they are confusing GBM
    // when UBWC flag is set which causes the allocated buffer to be plain NV12.
    if (memusage.flags & IMemAllocUsage::kPrivateAllocUbwc) {
      memusage.flags &= ~(IMemAllocUsage::kHwCameraRead |
          IMemAllocUsage::kHwCameraWrite);
    }

    MemAllocError ret = mem_alloc_interface_->AllocBuffer(
        handle,
        buf_width,
        buf_height,
        static_cast<int32_t>(camera_stream_.format),
        memusage,
        &current_buffer_stride_);
    if (MemAllocError::kAllocOk != ret) {
      return -ENOMEM;
    }
    idx = hw_buffer_allocated_;
    mem_alloc_slots_[idx] = handle;
    mem_alloc_buffers_.add(mem_alloc_slots_[idx], (NULL == streamBuffer));
    hw_buffer_allocated_++;
    CAMERA_INFO("%s: Allocated new buffer, total buffers allocated = %d",
        __func__, hw_buffer_allocated_);
  }

  if ((NULL == handle) || (0 > idx)) {
    CAMERA_ERROR("%s: Unable to allocate or find a free buffer!\n", __func__);
    return -ENOSYS;
  }

  if (NULL != streamBuffer) {
    streamBuffer->streamId = id_;
    streamBuffer->acquireFence = hidl_handle();
    streamBuffer->releaseFence = hidl_handle();
    streamBuffer->status = BufferStatus::OK;

    streamBuffer->buffer = hidl_handle(GetAllocBufferHandle(mem_alloc_slots_[idx]));

    streamBuffer->bufferId = idx;
    buffers_map[frame_number] = mem_alloc_slots_[idx];

    if (pending_buffer_count_ == 0 && status_ != STATUS_CONFIG_ACTIVE &&
        status_ != STATUS_RECONFIG_ACTIVE) {
      monitor_.ChangeStateToActive(monitor_id_);
    }

    hal_buffer_cnt_++;
    pending_buffer_count_++;

    CAMERA_DEBUG("%s: Stream(%d): pending_buffer_count_(%u)", __func__, id_,
        pending_buffer_count_);
  }

  return 0;
}

int32_t Camera3Stream::ConfigureLocked() {
  int32_t res;

  switch (status_) {
    case STATUS_RECONFIG_ACTIVE:
      res = CloseLocked();
      if (0 != res) {
        return res;
      }
      break;
    case STATUS_CONFIG_ACTIVE:
      break;
    default:
      CAMERA_ERROR("%s: Bad status: %d\n", __func__, status_);
      return -ENOSYS;
  }

  total_buffer_count_ = MAX(client_max_buffers_, stream_max_buffers_);
  pending_buffer_count_ = 0;
  hw_buffer_allocated_ = 0;
  is_stream_active_ = false;
  if (NULL != mem_alloc_slots_) {
    delete[] mem_alloc_slots_;
  }

  if (!mem_alloc_buffers_.isEmpty()) {
    assert(nullptr != mem_alloc_interface_);
    for (uint32_t i = 0; i < mem_alloc_buffers_.size(); i++) {
      mem_alloc_interface_->FreeBuffer(mem_alloc_buffers_.keyAt(i));
    }
    mem_alloc_buffers_.clear();
  }

  mem_alloc_slots_ = new IBufferHandle[total_buffer_count_];
  if (NULL == mem_alloc_slots_) {
    CAMERA_ERROR("%s: Unable to allocate buffer handles!\n", __func__);
    status_ = STATUS_ERROR;
    return -ENOMEM;
  }

  return 0;
}

int32_t Camera3Stream::CloseLocked() {
  switch (status_) {
    case STATUS_RECONFIG_ACTIVE:
    case STATUS_CONFIGURED:
      break;
    default:
      CAMERA_ERROR("%s: Stream %d is already closed!\n", __func__, id_);
      return -ENOTCONN;
  }

  if (pending_buffer_count_ > 0) {
    CAMERA_ERROR("%s: Can't disconnect with %zu buffers still dequeued!\n",
               __func__, pending_buffer_count_);
    for (uint32_t i = 0; i < mem_alloc_buffers_.size(); i++) {
      CAMERA_ERROR("%s: buffer[%d] = %p status: %d\n", __func__, i,
                 mem_alloc_buffers_.keyAt(i), mem_alloc_buffers_.valueAt(i));
    }
    PrintBuffersInfoLocked();
    return -ENOSYS;
  }

  assert(nullptr != mem_alloc_interface_);
  for (uint32_t i = 0; i < mem_alloc_buffers_.size(); i++) {
    mem_alloc_interface_->FreeBuffer(mem_alloc_buffers_.keyAt(i));
  }
  mem_alloc_buffers_.clear();

  status_ = (status_ == STATUS_RECONFIG_ACTIVE) ? STATUS_CONFIG_ACTIVE
                                                : STATUS_INTIALIZED;
  return 0;
}

void Camera3Stream::PrintBuffersInfo() {
  pthread_mutex_lock(&lock_);
  PrintBuffersInfoLocked();
  pthread_mutex_unlock(&lock_);
}

void Camera3Stream::PrintBuffersInfoLocked() {
  CAMERA_ERROR("%s: Stream id: %d dim: %ux%u, fmt: %d "
      "Buffers: HAL(%u) Client(%u) Pending(%d) Total(%d)", __func__, id_,
      camera_stream_.width, camera_stream_.height, (int)camera_stream_.format, hal_buffer_cnt_, client_buffer_cnt_,
      pending_buffer_count_, total_buffer_count_);
}

}  // namespace adaptor ends here

}  // namespace camera ends here
