/*
 * Copyright (c) 2018, 2021 The Linux Foundation. All rights reserved.
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

#include "allocator_hidl_interface.h"
#include <dlfcn.h>
#include "utils/camera_log.h"

using ::android::hardware::hidl_vec;
using ::android::hardware::Void;
using ::android::hardware::Return;
using ::android::hardware::hidl_handle;

const std::unordered_map<int32_t, uint64_t> HidlAllocUsage::usage_flag_map_ = {
    {IMemAllocUsage::kSwReadOften,      static_cast<uint64_t>(BufferUsage::CPU_READ_OFTEN)},
    {IMemAllocUsage::kVideoEncoder,     static_cast<uint64_t>(BufferUsage::VIDEO_ENCODER)},
    {IMemAllocUsage::kHwTexture,        static_cast<uint64_t>(BufferUsage::GPU_TEXTURE)},
    {IMemAllocUsage::kHwComposer,       static_cast<uint64_t>(BufferUsage::COMPOSER_CURSOR)},
    {IMemAllocUsage::kHwCameraRead,     static_cast<uint64_t>(BufferUsage::CAMERA_INPUT)},
    {IMemAllocUsage::kHwCameraWrite,    static_cast<uint64_t>(BufferUsage::CAMERA_OUTPUT)}};

uint64_t HidlAllocUsage::ToLocal(int32_t common) const {
  uint64_t local_usage = 0;
  for (auto &it : usage_flag_map_) {
    if (it.first & common) {
      local_usage |= it.second;
    }
  }
  return local_usage;
}

uint64_t HidlAllocUsage::ToLocal(MemAllocFlags common) const {
  uint64_t local_usage = 0;
  for (auto &it : usage_flag_map_) {
    if (it.first & common.flags) {
      local_usage |= it.second;
    }
  }
  return local_usage;
}

MemAllocFlags HidlAllocUsage::ToCommon(uint64_t local) const {
  MemAllocFlags common;
  common.flags = 0;
  for (auto &it : usage_flag_map_) {
    if (it.second & local) {
      common.flags |= it.first;
    }
  }
  return common;
}

buffer_handle_t &HidlAllocBuffer::GetNativeHandle() { return native_handle_;}

int HidlAllocBuffer::GetFD() {
  return -1;
}
PixelFormat HidlAllocBuffer::GetFormat() {
  return format_;
}
uint32_t HidlAllocBuffer::GetSize() {
  return 0;
}
uint32_t HidlAllocBuffer::GetWidth() {
  return width_;
}
uint32_t HidlAllocBuffer::GetHeight() {
  return height_;
}
uint32_t HidlAllocBuffer::GetStride() {
  return stride_;
}

MemAllocError HidlAllocDevice::AllocBuffer(IBufferHandle& handle, int32_t width,
                                           int32_t height, int32_t format,
                                           MemAllocFlags usage,
                                           uint32_t *stride) {
  assert(width && height);
  int32_t consumer_usage = HidlAllocUsage().ToLocal(usage);

  HidlAllocBuffer *buffer = new HidlAllocBuffer;
  handle = buffer;

  hidl_vec<uint32_t> descriptor;
  mapper::V3_0::IMapper::BufferDescriptorInfo descriptor_info {};

  descriptor_info.width = width;
  descriptor_info.height = height;
  descriptor_info.layerCount = 1;
  descriptor_info.format =
    static_cast<android::hardware::graphics::common::V1_2::PixelFormat>(format);

  descriptor_info.usage = static_cast<uint64_t>(consumer_usage |
      BufferUsage::CAMERA_OUTPUT);

  Return<void> ret = hidl_mapper_->createDescriptor(descriptor_info,
      [&descriptor](auto err, auto desc) {
        descriptor = desc;
      });

  if (!ret.isOk()) {
    CAMERA_ERROR("%s: Create descriptor failed.\n", __func__);
    goto alloc_fail;
  }

  {
    buffer_handle_t &buffer_handle = buffer->GetNativeHandle();
    ret = hidl_alloc_device_->allocate(descriptor, 1u,
        [&](auto err, uint32_t buf_stride,
            const ::android::hardware::hidl_vec<::android::hardware::hidl_handle>& buffers) {
          *stride = static_cast<uint32_t>(buf_stride);
          ret = hidl_mapper_->importBuffer(buffers[0], [&](const auto &err,
                                           const auto &buffer) {
              buffer_handle = static_cast<buffer_handle_t>(buffer);
              if (mapper::V3_0::Error::NONE != err) {
                CAMERA_ERROR("Mapper failed to import buffer\n");
              }
          });
        });

    if (!ret.isOk()) {
      CAMERA_ERROR("%s: Buffer allocation failed.\n", __func__);
      goto alloc_fail;
    }
  }

  buffer->format_ = static_cast<PixelFormat>(descriptor_info.format);
  buffer->width_ = descriptor_info.width;
  buffer->height_ = descriptor_info.height;
  buffer->stride_ = *stride;
  return MemAllocError::kAllocOk;

alloc_fail:
  CAMERA_ERROR("%s: Unable to allocate buffer: %d\n", __func__);
  delete buffer;
  handle = nullptr;
  return MemAllocError::kAllocFail;
}

MemAllocError HidlAllocDevice::ImportBuffer(IBufferHandle& handle,
                                          void* buffer_handle) {
  CAMERA_ERROR("%s: Not implemented", __func__);
  assert(0);
  return MemAllocError::kAllocOk;
}

MemAllocError HidlAllocDevice::FreeBuffer(IBufferHandle handle) {
  if (nullptr != handle) {
    HidlAllocBuffer *b = static_cast<HidlAllocBuffer *>(handle);
    assert(b != nullptr);
    const native_handle_t *buffer = b->GetNativeHandle();
    hidl_mapper_->freeBuffer(static_cast<void*>(const_cast<native_handle_t*>(buffer)));
    delete handle;
    handle = nullptr;
  }
  return MemAllocError::kAllocOk;
}

MemAllocError HidlAllocDevice::MapBuffer(const IBufferHandle& handle,
                                       int32_t start_x, int32_t start_y,
                                       int32_t width, int32_t height,
                                       MemAllocFlags usage, void **vaddr) {
  MemAllocError ret = MemAllocError::kAllocOk;

  HidlAllocBuffer *b = static_cast<HidlAllocBuffer *>(handle);
  assert(b != nullptr);

  int32_t consumer_usage = HidlAllocUsage().ToLocal(usage);
  auto local_usage = static_cast<uint64_t>(consumer_usage |
                              BufferUsage::CAMERA_OUTPUT);
  auto buffer = const_cast<native_handle_t*>(b->GetNativeHandle());

  hidl_handle fence_handle;
  mapper::V3_0::IMapper::Rect region {start_x, start_y, width, height};

  void* data = nullptr;
  hidl_mapper_->lock(buffer, local_usage, region, fence_handle,
      [&](const auto& error, const auto& data, int32_t bpp, int32_t bps) {
          *vaddr = data;
          if (mapper::V3_0::Error::NONE  != error) {
            ret = MemAllocError::kAllocFail;
            *vaddr = nullptr;
          }
      });

  return ret;
}

MemAllocError HidlAllocDevice::UnmapBuffer(const IBufferHandle& handle) {
  MemAllocError ret = MemAllocError::kAllocOk;

  HidlAllocBuffer *b = static_cast<HidlAllocBuffer *>(handle);
  assert(b != nullptr);
  auto buffer = const_cast<native_handle_t*>(b->GetNativeHandle());

  hidl_mapper_->unlock(buffer, [&](const auto& error, const auto& release_fence) {
    if (mapper::V3_0::Error::NONE != error) {
      CAMERA_ERROR("%s: failed to unlock buffer", __func__);
      ret = MemAllocError::kAllocFail;
    }
  });
  return ret;
}

MemAllocError HidlAllocDevice::Perform(const IBufferHandle& handle,
                                     AllocDeviceAction action, void* result) {
  HidlAllocBuffer *b = static_cast<HidlAllocBuffer *>(handle);
  if (nullptr == b) {
    return MemAllocError::kAllocFail;
  }

  switch (action) {
    case AllocDeviceAction::GetHeight:
      *static_cast<int32_t*>(result) = handle->GetHeight();
      return MemAllocError::kAllocOk;
    case AllocDeviceAction::GetStride:
      *static_cast<int32_t*>(result) = handle->GetStride();
      return MemAllocError::kAllocOk;
    case AllocDeviceAction::GetAlignedHeight:
      *static_cast<int32_t*>(result) = 0; //TODO: Extract real aligned value
      return MemAllocError::kAllocOk;
    case AllocDeviceAction::GetAlignedWidth:
      *static_cast<int32_t*>(result) = handle->GetStride();
      return MemAllocError::kAllocOk;
    default:
      CAMERA_ERROR("%s: Unrecognized action to perform.", __func__);
      return MemAllocError::kAllocFail;
  }
}

android::sp<allocator::V3_0::IAllocator> HidlAllocDevice::GetDevice() const {
  return hidl_alloc_device_;
}

HidlAllocDevice::HidlAllocDevice() {
  hidl_alloc_device_ = allocator::V3_0::IAllocator::getService();
  hidl_mapper_ = mapper::V3_0::IMapper::getService();

  assert(nullptr != hidl_alloc_device_.get());
  assert(nullptr != hidl_mapper_.get());
}

HidlAllocDevice::~HidlAllocDevice() {}
