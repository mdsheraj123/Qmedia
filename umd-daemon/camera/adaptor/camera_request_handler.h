/*
 * Copyright (c) 2016, 2021 The Linux Foundation. All rights reserved.
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
#ifndef CAMERA3REQUESTHANDLER_H_
#define CAMERA3REQUESTHANDLER_H_

#include <pthread.h>

#include <CameraMetadata.h>
#include <utils/KeyedVector.h>
#include <utils/List.h>
#include <chrono>
#include <mutex>
#include <thread>

#include "camera_types.h"
#include "camera_internal_types.h"
#include "utils/camera_thread.h"
#include "camera_defs.h"

using namespace android;

namespace camera {

namespace adaptor {

typedef std::function<void(const char *fmt, va_list args)> SetError;
typedef std::function<int32_t(uint32_t frameNumber, int32_t numBuffers,
                              CaptureResultExtras resultExtras)> MarkRequest;

class Camera3Monitor;

class Camera3RequestHandler : public ThreadHelper {
 public:
  Camera3RequestHandler(Camera3Monitor &monitor);
  virtual ~Camera3RequestHandler();

  int32_t Initialize(sp<ICameraDeviceSession> session,
                     ErrorCallback error_cb,
                     MarkRequest mark_cb, SetError set_error);

  int32_t SetRepeatingRequests(const RequestList &requests,
                               int64_t *lastFrameNumber = NULL);
  int32_t ClearRepeatingRequests(int64_t *lastFrameNumber = NULL);

  int32_t QueueRequestList(List<CaptureRequest> &requests,
                           int64_t *lastFrameNumber = NULL);

  int32_t QueueReprocRequestList(List<CaptureRequest> &requests,
                           int64_t *lastFrameNumber = NULL);

  int32_t Clear(int64_t *lastFrameNumber = NULL);

  void TogglePause(bool pause);

  void TogglePause(bool pause, bool &pending_request);

  bool IsStreamActive(Camera3Stream &stream);
  void FinishConfiguration(uint32_t batch_size);

  void RequestExit() override;
  void RequestExitAndWait() override;

 protected:
  bool ThreadLoop() override;

 private:
  int32_t GetRequest(CaptureRequest &request);
  int32_t SubmitRequest(CaptureRequest &nextRequest,
                        ::android::hardware::camera::device::V3_2::StreamBuffer *in_buf = nullptr);
  void ClearCaptureRequest(CaptureRequest &request);
  void HandleErrorRequest(::android::hardware::camera::device::V3_2::CaptureRequest &request,
                          CaptureRequest &nextRequest);

  bool WaitOnPause();
  void Resume();

  void SignalError(const char *fmt, ...);

  /**Not allowed */
  Camera3RequestHandler(const Camera3RequestHandler &);
  Camera3RequestHandler &operator=(const Camera3RequestHandler &);

  static const int64_t WAIT_TIMEOUT  = 50e6;  // 50 ms
  static const int64_t CLEAR_TIMEOUT = 1000e6; // 1000 ms

  ErrorCallback error_cb_;
  MarkRequest mark_cb_;
  SetError set_error_;
  sp<ICameraDeviceSession> camera_session_;

  pthread_mutex_t lock_;
  pthread_cond_t requests_signal_;
  pthread_cond_t current_request_signal_;
  RequestList requests_;
  RequestList streaming_requests_;

  CaptureRequest current_request_;
  CaptureRequest old_request_;

  bool configuration_update_;

  bool toggle_pause_state_;
  bool paused_state_;
  pthread_mutex_t pause_lock_;
  pthread_cond_t toggle_pause_signal_;
  pthread_cond_t pause_state_signal_;

  uint32_t current_frame_number_;
  uint32_t current_input_frame_number_;
  int64_t streaming_last_frame_number_;

  Camera3Monitor &monitor_;
  int32_t monitor_id_;
  uint32_t batch_size_;

  static void ReprocLoop(Camera3RequestHandler *ctx);
  RequestList       reproc_requests_;
  std::thread       worker_;
  std::atomic<bool> run_worker_;
  std::mutex        worker_lock_;
  QCondition        worker_signal_;

};

}  // namespace adaptor ends here

}  // namespace camera ends here

#endif /* CAMERA3REQUESTHANDLER_H_ */
