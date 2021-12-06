/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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

#include <chrono>
#include <condition_variable>
#include <cmath>
#include <iomanip>
#include <list>
#include <map>
#include <set>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>

#include <system/graphics.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <CameraMetadata.h>

#include "camera_recorder_params.h"
#include "utils/camera_log.h"
#include "utils/camera_condition.h"
#include "camera_memory_interface.h"

namespace camera {

using namespace android;
using namespace recorder;

typedef int32_t status_t;

const int64_t kWaitDelay = 2000000000;  // 2 sec

struct StreamBuffer {
  CameraBufferMetaData info;
  int64_t  timestamp;
  uint32_t frame_number;
  uint32_t camera_id;
  int32_t  stream_id;
  DataspaceFlags data_space;
  IBufferHandle handle;
  int32_t fd;
  uint32_t size;
  int32_t metafd;
  void *data;
  uint32_t flags;
  bool second_thumb;

  ::std::string ToString() const {
    ::std::stringstream stream;
    stream << "camera[" << camera_id << "] ";
    stream << "stream[" << stream_id << "] ";
    stream << "data[" << data << "] ";
    stream << "fd[" << fd << "] ";
    stream << "size[" << size << "] ";
    stream << "timestamp[" << timestamp << "] ";
    stream << "flags[" << ::std::setbase(16) << flags << ::std::setbase(10)
           << "]";
    stream << "second_thumb[" << second_thumb << "] ";
    return stream.str();
  }
};

};  // namespace camera.

#ifdef QCAMERA3_TAG_LOCAL_COPY
namespace qcamera {
// With the new camera backend design coming in Android-O ,
// vendor tags names, querying mechanism and file location
// have changed. The following code is being added to unblock
// compilation, till the new tags and their querying mechanism
// are being implemented.
#define QCAMERA3_EXPOSURE_METER_AVAILABLE_MODES  0x80080001
#define QCAMERA3_EXPOSURE_METER  0x80080000
#define QCAMERA3_USE_SATURATION  0x80070000
#define QCAMERA3_SELECT_PRIORITY  0x80060001
#define QCAMERA3_USE_ISO_EXP_PRIORITY  0x80060000
#define QCAMERA3_VENDOR_STREAM_CONFIGURATION_RAW_ONLY_MODE  0x8000
#define QCAMERA3_VENDOR_STREAM_CONFIGURATION_PP_DISABLED_MODE  0x8001
#define QCAMERA3_DUALCAM_SYNCHRONIZED_REQUEST  0x800b0005
#define QCAMERA3_DUALCAM_LINK_IS_MAIN  0x800b0001
#define QCAMERA3_DUALCAM_LINK_RELATED_CAMERA_ID  0x800b0002
#define QCAMERA3_DUALCAM_LINK_ENABLE  0x800b0000
#define QCAMERA3_DUALCAM_LINK_CAMERA_ROLE_BAYER  0x1
#define QCAMERA3_DUALCAM_LINK_CAMERA_ROLE  0x800b0003
#define QCAMERA3_DUALCAM_LINK_3A_360_CAMERA  0x3
#define QCAMERA3_DUALCAM_LINK_3A_SYNC_MODE  0x800b0004
#define QCAMERA3_TARGET_LUMA  0x801e0000
#define QCAMERA3_CURRENT_LUMA  0x801e0001
#define QCAMERA3_LUMA_RANGE  0x801e0002
#define QCAMERA3_AVAILABLE_VIDEO_HDR_MODES  0x800f0001
#define QCAMERA3_VIDEO_HDR_MODE  0x800f0000
#define QCAMERA3_VIDEO_HDR_MODE_ON  0x1
#define QCAMERA3_VIDEO_HDR_MODE_OFF  0x0
#define QCAMERA3_LCAC_PROCESSING_ENABLE  0x801f0000
#define QCAMERA3_IR_MODE  0x80100000
#define QCAMERA3_IR_AVAILABLE_MODES  0x80100001
#define QCAMERA3_IR_MODE_OFF  0x0
#define QCAMERA3_IR_MODE_ON  0x1
#define QCAMERA3_AVAILABLE_BINNING_CORRECTION_MODES  0x80160001
#define QCAMERA3_BINNING_CORRECTION_MODE_ON  0x1
#define QCAMERA3_BINNING_CORRECTION_MODE_OFF  0x0
#define QCAMERA3_BINNING_CORRECTION_MODE  0x80160000
#define QCAMERA3_SHARPNESS_STRENGTH  0x80140000
#define QCAMERA3_SHARPNESS_RANGE  0x80140001
#define QCAMERA3_WNR_RANGE  0x80180000
#define QCAMERA3_TNR_INTENSITY  0x801a0000
#define QCAMERA3_TNR_MOTION_DETECTION_SENSITIVITY  0x801a0001
#define QCAMERA3_TNR_TUNING_RANGE  0x801a0002
#define QCAMERA3_HISTOGRAM_STATS  0x80150003
#define QCAMERA3_HISTOGRAM_BUCKETS  0x80150001
#define QCAMERA3_HISTOGRAM_MODE  0x80150000
#define QCAMERA3_HISTOGRAM_MODE_OFF  0x0
#define QCAMERA3_HISTOGRAM_MODE_ON  0x1
#define QCAMERA3_EXPOSURE_DATA_ON  0x1
#define QCAMERA3_EXPOSURE_DATA_OFF  0x0
#define QCAMERA3_AWB_ROI_COLOR  0x801d0000
#define QCAMERA3_EXPOSURE_DATA_ENABLE  0x80190000
#define QCAMERA3_EXPOSURE_DATA_REGION_H_NUM  0x80190001
#define QCAMERA3_EXPOSURE_DATA_REGION_V_NUM  0x80190002
#define QCAMERA3_EXPOSURE_DATA_REGION_PIXEL_CNT  0x80190003
#define QCAMERA3_EXPOSURE_DATA_REGION_HEIGHT  0x80190004
#define QCAMERA3_EXPOSURE_DATA_REGION_WIDTH  0x80190005
#define QCAMERA3_EXPOSURE_DATA_R_SUM  0x80190006
#define QCAMERA3_EXPOSURE_DATA_B_SUM  0x80190007
#define QCAMERA3_EXPOSURE_DATA_GR_SUM  0x80190008
#define QCAMERA3_EXPOSURE_DATA_GB_SUM  0x80190009
#define QCAMERA3_EXPOSURE_DATA_R_NUM  0x8019000a
#define QCAMERA3_EXPOSURE_DATA_B_NUM  0x8019000b
#define QCAMERA3_EXPOSURE_DATA_GR_NUM  0x8019000c
#define QCAMERA3_EXPOSURE_DATA_GB_NUM  0x8019000d
#define QCAMERA3_CURRENT_LUX_IDX  0x801e0003
#define QCAMERA3_CDS_MODE  0x80010000
#define QCAMERA3_IS_H_MARGIN_CFG 0x80010001
#define QCAMERA3_IS_V_MARGIN_CFG 0x80010002

// QCAMERA3_ISO_EXP_PRIORITY
typedef enum qcamera3_ext_iso_mode {
    QCAMERA3_ISO_MODE_AUTO,
    QCAMERA3_ISO_MODE_DEBLUR,
    QCAMERA3_ISO_MODE_100,
    QCAMERA3_ISO_MODE_200,
    QCAMERA3_ISO_MODE_400,
    QCAMERA3_ISO_MODE_800,
    QCAMERA3_ISO_MODE_1600,
    QCAMERA3_ISO_MODE_3200,
} qcamera3_ext_iso_mode_t;
};  // namespace qcamera.
#endif  // QCAMERA3_TAG_LOCAL_COPY

