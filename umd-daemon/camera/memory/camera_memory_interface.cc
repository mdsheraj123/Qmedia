/*
 * Copyright (c) 2018, 2019, 2021 The Linux Foundation. All rights reserved.
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

#include "camera_memory_interface.h"
#include "utils/camera_log.h"

const int IMemAllocUsage::kHwCameraZsl      = (1 << 0);
const int IMemAllocUsage::kPrivateAllocUbwc = (1 << 1);
const int IMemAllocUsage::kPrivateIommUHeap = (1 << 2);
const int IMemAllocUsage::kPrivateMmHeap    = (1 << 3);
const int IMemAllocUsage::kPrivateUncached  = (1 << 4);
const int IMemAllocUsage::kProtected        = (1 << 5);
const int IMemAllocUsage::kSwReadOften      = (1 << 6);
const int IMemAllocUsage::kSwWriteOften     = (1 << 7);
const int IMemAllocUsage::kVideoEncoder     = (1 << 8);
const int IMemAllocUsage::kHwFb             = (1 << 9);
const int IMemAllocUsage::kHwTexture        = (1 << 10);
const int IMemAllocUsage::kHwRender         = (1 << 11);
const int IMemAllocUsage::kHwComposer       = (1 << 12);
const int IMemAllocUsage::kHwCameraRead     = (1 << 13);
const int IMemAllocUsage::kHwCameraWrite    = (1 << 14);

IAllocDevice *AllocDeviceFactory::CreateAllocDevice() {
  return new HidlAllocDevice;
}

void AllocDeviceFactory::DestroyAllocDevice(IAllocDevice* alloc_device_interface) {
  delete alloc_device_interface;
}

const IMemAllocUsage &AllocUsageFactory::GetAllocUsage() {
  static const HidlAllocUsage x = HidlAllocUsage();
  return x;
}


buffer_handle_t &GetAllocBufferHandle(const IBufferHandle &handle) {
  HidlAllocBuffer *b = static_cast<HidlAllocBuffer *>(handle);
  assert(b != nullptr);
  return b->GetNativeHandle();
}