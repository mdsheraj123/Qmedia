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

#include <android/hardware/camera/device/3.2/types.h>
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <android/hardware/camera/device/3.2/ICameraDeviceCallback.h>
#include <android/hardware/camera/device/3.2/ICameraDeviceSession.h>

#include <android/hardware/camera/provider/2.4/ICameraProvider.h>

#include <android/hardware/graphics/allocator/3.0/IAllocator.h>
#include <android/hardware/graphics/mapper/3.0/IMapper.h>
#include <android/hardware/graphics/mapper/3.0/types.h>
#include <CameraMetadata.h>

using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;

using namespace ::android::hardware::camera;

using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;

using ::android::hardware::Void;
using ::android::hardware::Return;

using namespace ::android::hardware::graphics::allocator::V3_0;
using namespace ::android::hardware::graphics::mapper::V3_0;

using ::android::hardware::graphics::common::V1_0::PixelFormat;

using ::android::hardware::camera::provider::V2_4::ICameraProvider;
using ::android::hardware::camera::provider::V2_4::ICameraProviderCallback;

using ::android::hardware::camera::device::V3_2::ICameraDevice;
using ::android::hardware::camera::device::V3_2::ICameraDeviceSession;
using ::android::hardware::camera::device::V3_2::ICameraDeviceCallback;
using ::android::hardware::camera::device::V3_2::StreamConfigurationMode;
using ::android::hardware::camera::device::V3_2::HalStreamConfiguration;
using ::android::hardware::camera::device::V3_2::RequestTemplate;
using ::android::hardware::camera::device::V3_2::NotifyMsg;
using ::android::hardware::camera::device::V3_2::ShutterMsg;
using ::android::hardware::camera::device::V3_2::Stream;
using ::android::hardware::camera::device::V3_2::StreamType;
using ::android::hardware::camera::device::V3_2::BufferStatus;
using ::android::hardware::camera::device::V3_2::ErrorCode;
using ::android::hardware::camera::device::V3_2::ErrorMsg;
using ::android::hardware::camera::device::V3_2::BufferUsageFlags;
using ::android::hardware::camera::device::V3_2::DataspaceFlags;
using ::android::hardware::camera::device::V3_2::StreamRotation;
using android::hardware::camera::device::V3_2::RequestTemplate;