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

#pragma once

#include "camera_memory_interface.h"

#include <android/hardware/graphics/allocator/3.0/IAllocator.h>
#include <android/hardware/graphics/mapper/3.0/IMapper.h>
#include <android/hardware/graphics/mapper/3.0/types.h>

using ::android::hardware::graphics::common::V1_1::BufferUsage;

class HidlAllocUsage : public IMemAllocUsage {
 public:
  uint64_t ToLocal(int32_t common) const;
  uint64_t ToLocal(MemAllocFlags common) const;
  MemAllocFlags ToCommon(uint64_t local) const;

 private:
  static const std::unordered_map<int32_t, uint64_t> usage_flag_map_;
};

class HidlAllocBuffer : public IBufferInterface {
 public:
  HidlAllocBuffer(){};
  ~HidlAllocBuffer(){};
  buffer_handle_t & GetNativeHandle();
  int GetFD() override;
  PixelFormat GetFormat() override;
  uint32_t GetSize() override;
  uint32_t GetWidth() override;
  uint32_t GetHeight() override;
  uint32_t GetStride() override;

  uint32_t stride_;
  int32_t width_;
  int32_t height_;
  PixelFormat format_;

 private:
  buffer_handle_t native_handle_;
};

class HidlAllocDevice : public IAllocDevice {
 public:
  HidlAllocDevice();

  ~HidlAllocDevice();

  MemAllocError AllocBuffer(IBufferHandle& handle, int32_t width,
                            int32_t height, int32_t format,
                            MemAllocFlags usage, uint32_t* stride) override;

  MemAllocError ImportBuffer(IBufferHandle& handle,
                             void* buffer_handle) override;

  MemAllocError FreeBuffer(IBufferHandle handle) override;

  MemAllocError Perform(const IBufferHandle& handle, AllocDeviceAction action,
                        void* result) override;

  MemAllocError MapBuffer(const IBufferHandle& handle, int32_t start_x,
                          int32_t start_y, int32_t width, int32_t height,
                          MemAllocFlags usage, void** vaddr) override;

  MemAllocError UnmapBuffer(const IBufferHandle& handle) override;

  android::sp<allocator::V3_0::IAllocator> GetDevice() const;

 private:
  android::sp<allocator::V3_0::IAllocator> hidl_alloc_device_;
  android::sp<mapper::V3_0::IMapper> hidl_mapper_;
};
