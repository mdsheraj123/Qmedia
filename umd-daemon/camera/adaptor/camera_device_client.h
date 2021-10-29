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

#ifndef CAMERA3DEVICE_H_
#define CAMERA3DEVICE_H_

#include <pthread.h>
#include <CameraMetadata.h>
#include <utils/KeyedVector.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <mutex>

#include <fmq/MessageQueue.h>

#ifdef TARGET_USES_GBM
#include <gbm.h>
#include <gbm_priv.h>
#include <fcntl.h>
#endif

#include "camera_types.h"
#include "camera_internal_types.h"
#include "camera_stream.h"
#include "camera_request_handler.h"
#include "camera_monitor.h"
#include "camera_prepare_handler.h"

#include "camera_defs.h"

#define CAMERA_TEMPLATE_COUNT 10

using namespace android;

namespace camera {

namespace adaptor {

using ::android::hardware::MessageQueue;
using ::android::hardware::kSynchronizedReadWrite;
using ResultMetadataQueue = MessageQueue<uint8_t, kSynchronizedReadWrite>;

class Camera3DeviceClient : public ICameraDeviceCallback {
 public:
  Camera3DeviceClient(CameraClientCallbacks clientCb);
  virtual ~Camera3DeviceClient();

  int32_t Initialize();

  int32_t OpenCamera(uint32_t idx);
  int32_t BeginConfigure() { return 0; }

  int32_t EndConfigure(const StreamConfiguration& stream_config
                       = StreamConfiguration());

  int32_t DeleteStream(int streamId, bool cache);
  int32_t CreateStream(const CameraStreamParameters &outputConfiguration);
  int32_t CreateInputStream(
      const CameraInputStreamParameters &inputConfiguration);

  int32_t CreateDefaultRequest(int templateId, CameraMetadata *request);
  int32_t CreateDefaultRequest(RequestTemplate templateId, CameraMetadata *request);
  int32_t SubmitRequest(Camera3Request request, bool streaming = false,
                        int64_t *lastFrameNumber = NULL);
  int32_t SubmitRequestList(std::list<Camera3Request> requests,
                            bool streaming = false,
                            int64_t *lastFrameNumber = NULL);
  int32_t ReturnStreamBuffer(StreamBuffer buffer);
  int32_t CancelRequest(int requestId, int64_t *lastFrameNumber = NULL);

  int32_t GetCameraInfo(uint32_t idx, CameraMetadata *info);
  int32_t GetNumberOfCameras() { return number_of_cameras_; }
  const std::vector<int32_t> GetRequestIds(){ return current_request_ids_; }
  int32_t WaitUntilIdle();

  int32_t Flush(int64_t *lastFrameNumber = NULL);
  int32_t Prepare(int streamId);
  int32_t TearDown(int streamId);

  static int32_t LoadHWModule(const char *moduleId,
                              const struct hw_module_t **pHmi);

 private:
  std::vector<int32_t> current_request_ids_;
  typedef enum State_t {
    STATE_ERROR,
    STATE_NOT_INITIALIZED,
    STATE_CLOSED,
    STATE_NOT_CONFIGURED,
    STATE_CONFIGURED,
    STATE_RUNNING
  } State;

  friend class Camera3PrepareHandler;
  friend class Camera3RequestHandler;
  friend class Camera3Monitor;
  friend class Camera3Gtest;
  friend class DualCamera3Gtest;

  int32_t AddRequestListLocked(const List<const CameraMetadata> &requests,
                               bool streaming, int64_t *lastFrameNumber = NULL);

  void HandleCaptureResult(const ::android::hardware::camera::device::V3_2::CaptureResult &result);
  void NotifyError(const ErrorMsg &msg);
  void NotifyShutter(const ShutterMsg &msg);
  void RemovePendingRequestLocked(uint32_t frameNumber);
  void ReturnOutputBuffers(const ::android::hardware::camera::device::V3_2::StreamBuffer *outputBuffers,
                           size_t numBuffers, int64_t timestamp,
                           int64_t frame_number);
  void SendCaptureResult(CameraMetadata &pendingMetadata,
                         CaptureResultExtras &resultExtras,
                         CameraMetadata &collectedPartialResult,
                         uint32_t frameNumber);

  void NotifyStatus(bool idle);
  int32_t WaitUntilDrainedLocked();
  void InternalUpdateStatusLocked(State state);
  int32_t InternalPauseAndWaitLocked();
  int32_t InternalResumeLocked();
  int32_t WaitUntilStateThenRelock(bool active, int64_t timeout);

  int32_t CaclulateBlobSize(int32_t width, int32_t height);
  int32_t QueryMaxBlobSize(int32_t &maxJpegSizeWidth,
                           int32_t &maxJpegSizeHeight);

  int32_t ConfigureStreams(const StreamConfiguration& stream_config
                           = StreamConfiguration());

  int32_t ConfigureStreamsLocked();

  void SetErrorState(const char *fmt, ...);
  void SetErrorStateV(const char *fmt, va_list args);
  void SetErrorStateLocked(const char *fmt, ...);
  void SetErrorStateLockedV(const char *fmt, va_list args);

  Return<void> processCaptureResult(
      const hidl_vec<::android::hardware::camera::device::V3_2::CaptureResult>& results) override;
  Return<void> notify(
      const hidl_vec<NotifyMsg>& messages) override;

  int32_t MarkPendingRequest(uint32_t frameNumber, int32_t numBuffers,
                             CaptureResultExtras resultExtras);

  bool HandlePartialResult(uint32_t frameNumber, const CameraMetadata &partial,
                           const CaptureResultExtras &resultExtras);

  /**Not allowed */
  Camera3DeviceClient(const Camera3DeviceClient &);
  Camera3DeviceClient &operator=(const Camera3DeviceClient &);

  template <typename T>
  bool QueryPartialTag(const CameraMetadata &result, int32_t tag, T *value,
                       uint32_t frameNumber);
  template <typename T>
  bool UpdatePartialTag(CameraMetadata &result, int32_t tag, const T *value,
                        uint32_t frameNumber);

  int32_t GetRequestListLocked(const List<const CameraMetadata> &metadataList,
                               RequestList *requestList,
                               RequestList *requestListReproc);
  int32_t GenerateCaptureRequestLocked(const CameraMetadata &request,
                                       CaptureRequest &captureRequest);

  StreamConfigurationMode GetOpMode();

  pthread_mutex_t pending_requests_lock_;
  PendingRequestVector pending_requests_vector_;
  PendingRequestVector pending_error_requests_vector_;

  pthread_mutex_t lock_;
  CameraClientCallbacks client_cb_;

  String8 last_error_;
  uint32_t id_;

  State state_;
  bool flush_on_going_;

  KeyedVector<int, Camera3Stream *> streams_;
  Vector<Camera3Stream *> deleted_streams_;

  int next_stream_id_;
  bool reconfig_;

  CameraMetadata request_templates_[CAMERA_TEMPLATE_COUNT];
  static const int32_t JPEG_BUFFER_SIZE_MIN =
      256 * 1024 + 6 /*sizeof(camera3_jpeg_blob)*/;

  sp<ICameraProvider> camera_provider_;
  sp<ICameraDevice> camera_device_;
  sp<ICameraDeviceSession> camera_session_;
  std::unique_ptr<ResultMetadataQueue> result_metadata_queue_;

  std::vector<std::string> camera_device_names_;

  uint32_t number_of_cameras_;
  CameraMetadata device_info_;
  IAllocDevice* alloc_device_interface_;

  Vector<int32_t> repeating_requests_;
  int32_t next_request_id_;
  uint32_t frame_number_;
  uint32_t next_shutter_frame_number_;
  uint32_t next_shutter_input_frame_number_;
  uint32_t partial_result_count_;
  bool is_partial_result_supported_;
  uint32_t next_result_frame_number_;
  uint32_t next_result_input_frame_number_;
  Camera3Monitor monitor_;
  Camera3RequestHandler request_handler_;

  bool pause_state_notify_;
  Vector<State> current_state_updates_;
  int state_listeners_;
  pthread_cond_t state_updated_;
  static const int64_t WAIT_FOR_SHUTDOWN = 5e9;  // 5 sec.
  static const int64_t WAIT_FOR_RUNNING = 1e9;   // 1 sec.

  bool is_hfr_supported_;
  bool is_raw_only_;
  bool hfr_mode_enabled_;
  uint32_t cam_feature_flags_;
  uint32_t fps_sensormode_index_;
  int32_t frame_rate_range_[2];
  Camera3PrepareHandler prepare_handler_;
  Camera3InputStream input_stream_;
  uint32_t batch_size_;
  static uint32_t client_count_;
};

}  // namespace adaptor ends here

}  // namespace camera ends here

#endif /* CAMERA3DEVICE_H_ */
