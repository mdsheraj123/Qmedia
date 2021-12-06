/*
 * Copyright (c) 2016, 2018, 2021 The Linux Foundation. All rights reserved.
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

#ifndef CAMERA3INTERNALTYPES_H_
#define CAMERA3INTERNALTYPES_H_

#include <map>
#include <CameraMetadata.h>

using namespace android;

namespace camera {

namespace adaptor {

class Camera3Stream;

typedef struct {
  int32_t stream_id;
  GetInputBuffer get_input_buffer;
  ReturnInputBuffer return_input_buffer;
  std::unordered_map <buffer_handle_t, IBufferHandle> buffers_map;
  uint32_t input_buffer_cnt;
  uint32_t width;
  uint32_t height;
  int32_t format;
  StreamType stream_type;
} Camera3InputStream;

typedef struct CaptureRequest_t {
  CameraMetadata metadata;
  Vector<Camera3Stream *> streams;
  CaptureResultExtras resultExtras;
  Camera3InputStream *input;
} CaptureRequest;

typedef List<CaptureRequest> RequestList;

struct PendingRequest {
  int64_t shutterTS;
  int64_t sensorTS;
  CaptureResultExtras resultExtras;
  int status;
  bool isMetaPresent;
  int buffersRemaining;
  CameraMetadata pendingMetadata;
  Vector<::android::hardware::camera::device::V3_2::StreamBuffer> pendingBuffers;

  struct PartialResult {
    bool partial3AReceived;
    CameraMetadata composedResult;

    PartialResult() : partial3AReceived(false) {}
  } partialResult;

  PendingRequest()
      : shutterTS(0),
        sensorTS(0),
        status(OK),
        isMetaPresent(false),
        buffersRemaining(0) {}

  PendingRequest(int numBuffers, CaptureResultExtras extras)
      : shutterTS(0),
        sensorTS(0),
        resultExtras(extras),
        status(OK),
        isMetaPresent(false),
        buffersRemaining(numBuffers) {}
};

typedef std::map<uint32_t, PendingRequest> PendingRequestVector;

}  // namespace adaptor ends here

}  // namespace camera ends here

#endif /* CAMERA3INTERNALTYPES_H_ */
