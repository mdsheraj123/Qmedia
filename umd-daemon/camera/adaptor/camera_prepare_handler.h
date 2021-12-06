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
#ifndef CAMERA3PREPAREHANDLER_H_
#define CAMERA3PREPAREHANDLER_H_

#include <pthread.h>
#include <utils/List.h>

#include "camera_types.h"
#include "camera_internal_types.h"

#include "utils/camera_thread.h"

using namespace android;

namespace camera {

namespace adaptor {

class Camera3Stream;

class Camera3PrepareHandler : public ThreadHelper {
 public:
  Camera3PrepareHandler();
  virtual ~Camera3PrepareHandler();

  void SetPrepareCb(PreparedCallback prepare_cb) {prepare_cb_ = prepare_cb;};
  int32_t Prepare(Camera3Stream *stream);
  int32_t Clear();

 protected:
  bool ThreadLoop() override;

 private:

  /**Not allowed */
  Camera3PrepareHandler(const Camera3PrepareHandler &);
  Camera3PrepareHandler &operator=(const Camera3PrepareHandler &);

  PreparedCallback prepare_cb_;
  pthread_mutex_t lock_;
  List<Camera3Stream *> streams_;
  bool prepare_running_;
  bool abort_prepare_;
  Camera3Stream *prepared_stream_;
};

}  // namespace adaptor ends here

}  // namespace camera ends here

#endif /* CAMERA3PREPAREHANDLER_H_ */
