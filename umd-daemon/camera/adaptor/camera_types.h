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

#ifndef CAMERA3TYPES_H_
#define CAMERA3TYPES_H_
#include <functional>

#include <CameraMetadata.h>

#include "utils/camera_common_utils.h"

using namespace android;

namespace camera {

namespace adaptor {

// Please note that you can call all "Camera3DeviceClient" API methods
// from the same context of this callback.
typedef std::function<void(StreamBuffer buffer)> StreamCallback;

enum class CamFeatureFlag : uint32_t {
  kNone = 0,                   /// No Feature is on.
  kEIS = 1 << 0,               /// EIS is on.
  kHDR = 1 << 1,               /// HDR is on.
  kLDC = 1 << 2,               /// LDC is on.
  kLCAC = 1 << 3,              /// LCAC is on.
  kForceSensorMode = 1 << 4,   /// Force Sensor Mode is on.
};

#define FORCE_SENSOR_MODE_MASK      (0x00FF0000)
#define FORCE_SENSOR_MODE_DATA(idx) ((idx + 1) << 16)

struct CameraStreamParameters {
  uint32_t width;
  uint32_t height;
  PixelFormat format;
  DataspaceFlags data_space;
  StreamRotation rotation;
  MemAllocFlags allocFlags;
  uint32_t bufferCount;
  StreamCallback cb;
  uint32_t cam_feature_flags;
  CameraStreamParameters() :
        width(0), height(0),
        data_space(static_cast<DataspaceFlags>
          (::android::hardware::graphics::common::V1_0::Dataspace::UNKNOWN)),
        rotation(StreamRotation::ROTATION_0),
        allocFlags(), bufferCount(0), cb(nullptr),
        cam_feature_flags(static_cast<uint32_t>(CamFeatureFlag::kNone)) {}
};

struct StreamConfiguration {
  bool is_constrained_high_speed;
  bool is_raw_only;
  uint32_t batch_size;
  uint32_t fps_sensormode_index;
  CameraStreamParameters *params;
  int32_t frame_rate_range[2];

  StreamConfiguration()
    :  is_constrained_high_speed(false), is_raw_only(false), batch_size(1),
       fps_sensormode_index(0), params(nullptr), frame_rate_range{} {}
};

typedef struct Camera3Request_t {
  CameraMetadata metadata;
  Vector<int32_t> streamIds;
} Camera3Request;

typedef struct {
  int32_t  requestId;
  int32_t  burstId;
  uint32_t frameNumber;
  int32_t  partialResultCount;
  bool     input;
} CaptureResultExtras;

typedef struct {
  CameraMetadata metadata;
  CaptureResultExtras resultExtras;
} CaptureResult;

enum CameraErrorCode {
  ERROR_CAMERA_INVALID_ERROR = 0, // All other invalid error codes
  ERROR_CAMERA_DEVICE = 1,        // Un-recoverable camera error
  ERROR_CAMERA_REQUEST = 2,       // Error during request processing
  ERROR_CAMERA_RESULT = 3,        // Error when generating request result
  ERROR_CAMERA_BUFFER = 4,        // Error during buffer processing
};

// Notifies about all sorts of errors that can happen during camera operation
typedef std::function<
    void(CameraErrorCode errorCode, const CaptureResultExtras &resultExtras)>
    ErrorCallback;
// Notifies the client that camera is idle with no pending requests
typedef std::function<void()> IdleCallback;
// Notifies about a shutter event
typedef std::function<void(const CaptureResultExtras &resultExtras,
                           int64_t timestamp)> ShutterCallback;
// Notifies when stream buffers got allocated
typedef std::function<void(int streamId)> PreparedCallback;
// Notifies about a new capture result
typedef std::function<void(const CaptureResult &result)> ResultCallback;

// Please note that these callbacks shouldn't get blocked for long durations.
// Also very important is to not to try and call "Camera3DeviceClient" API
// methods
// from the same context of these callbacks. This can lead to deadlocks!
typedef struct {
  ErrorCallback errorCb;
  IdleCallback idleCb;
  ShutterCallback shutterCb;
  PreparedCallback peparedCb;
  ResultCallback resultCb;
} CameraClientCallbacks;

//Please note that this callbacks need to return as fast as possible
//otherwise the camera framerate can be affected.
typedef std::function<void(StreamBuffer &buffer)> GetInputBuffer;
typedef std::function<void(StreamBuffer &buffer)> ReturnInputBuffer;

typedef struct {
  uint32_t width;
  uint32_t height;
  int32_t format;
  GetInputBuffer get_input_buffer;
  ReturnInputBuffer return_input_buffer;
} CameraInputStreamParameters;

enum {
  NO_IN_FLIGHT_REPEATING_FRAMES = -1,
};

}  // namespace adaptor ends here

}  // namespace camera ends here

#endif /* CAMERA3TYPES_H_ */
