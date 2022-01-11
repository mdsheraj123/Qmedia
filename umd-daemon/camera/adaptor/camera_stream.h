/*
 * Copyright (c) 2016, 2018, 2019, 2021 The Linux Foundation. All rights reserved.
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

#ifndef CAMERA3STREAM_H_
#define CAMERA3STREAM_H_

#include <pthread.h>

#include <utils/String8.h>
#include <utils/Vector.h>
#include <utils/KeyedVector.h>

#include "camera_types.h"
#include "camera_memory_interface.h"

#include "camera_defs.h"

using namespace android;

namespace camera {

namespace adaptor {

class Camera3Monitor;

class Camera3Stream {

 public:
  Camera3Stream(int id, size_t maxSize,
                const CameraStreamParameters &outputConfiguration,
                IAllocDevice *device, Camera3Monitor &monitor);
  virtual ~Camera3Stream();

  Stream *BeginConfigure();
  int32_t EndConfigure();
  int32_t AbortConfigure();
  bool IsConfigureActive();

  int32_t BeginPrepare();
  int32_t PrepareBuffer();
  int32_t EndPrepare();
  bool IsPrepareActive();

  int32_t GetBuffer(::android::hardware::camera::device::V3_2::StreamBuffer *buffer, int64_t frame_number);
  int32_t ReturnBuffer(const StreamBuffer &buffer);
  std::unordered_map <int64_t, IBufferHandle> buffers_map;

  void ReturnBufferToClient(const ::android::hardware::camera::device::V3_2::StreamBuffer &buffer,
                            int64_t timestamp, int64_t frame_number);

  int32_t Close();

  int GetId() const { return id_; }

  bool IsStreamActive();

  void PrintBuffersInfo();

  int32_t TearDown();

  Stream camera_stream_;
  uint32_t stream_max_buffers_;

 private:
  int32_t ConfigureLocked();
  int32_t GetBufferLocked(::android::hardware::camera::device::V3_2::StreamBuffer *buffer = NULL,
      int64_t frame_number = -1);
  int32_t ReturnBufferLocked(const StreamBuffer &buffer);
  uint32_t GetBufferCountLocked() { return total_buffer_count_; }
  uint32_t GetPendingBufferCountLocked() { return pending_buffer_count_; }
  int32_t CloseLocked();

  int32_t EndPrepareLocked();
  int32_t PopulateMetaInfo(CameraBufferMetaData &info,
                           IBufferHandle &handle);

  void PrintBuffersInfoLocked();

  /**Not allowed */
  Camera3Stream(const Camera3Stream &);
  Camera3Stream &operator=(const Camera3Stream &);

  IAllocDevice* mem_alloc_interface_;

  uint32_t current_buffer_stride_;
  const int32_t id_;
  // Should be zero for non-blob formats
  const uint32_t max_size_;

  typedef enum Status_t {
    STATUS_ERROR,
    STATUS_INTIALIZED,
    STATUS_CONFIG_ACTIVE,
    STATUS_RECONFIG_ACTIVE,
    STATUS_CONFIGURED,
    STATUS_PREPARE_ACTIVE,
  } Status;

  pthread_mutex_t lock_;

  Status status_;
  uint32_t total_buffer_count_;
  uint32_t pending_buffer_count_;
  uint32_t hal_buffer_cnt_;
  uint32_t client_buffer_cnt_;

  StreamCallback callbacks_;
  MemAllocFlags old_usage_, client_usage_;
  uint32_t old_max_buffers_, client_max_buffers_;
  pthread_cond_t output_buffer_returned_signal_;
  static const int64_t BUFFER_WAIT_TIMEOUT = 1e9;  // 1 sec.

  KeyedVector<IBufferHandle , bool> mem_alloc_buffers_;
  IBufferHandle *mem_alloc_slots_;
  uint32_t hw_buffer_allocated_;

  Camera3Monitor &monitor_;
  int32_t monitor_id_;

  bool is_stream_active_;
  uint32_t prepared_buffers_count_;
};

}  // namespace adaptor ends here

}  // namespace camera ends here

#endif /* CAMERA3STREAM_H_ */
