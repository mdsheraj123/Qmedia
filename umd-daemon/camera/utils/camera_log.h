/*
 * Copyright (c) 2016-2017, 2021 The Linux Foundation. All rights reserved.
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

#include <utils/Log.h>
#include <cutils/properties.h>

#undef assert
// Invalid ptr operation just to get backtraces into logcat.
// TODO: Use "libunwind" to get proper traces.
#define assert(condition) do { \
  if (!(condition)) { \
    int *p = 0; \
    CAMERA_ERROR("assert(%s) at %s:%d", #condition, __FILE__, __LINE__); \
    *p = 4; \
    ALOGE("%p", p); \
  } \
} while (0)

#define LOG_LEVEL_KPI

// INFO, ERROR and WARN logs are enabled by default
#define CAMERA_INFO(fmt, args...)  ALOGI(fmt, ##args)
#define CAMERA_WARN(fmt, args...)  ALOGW(fmt, ##args)
#define CAMERA_ERROR(fmt, args...) ALOGE(fmt, ##args)

static inline void unused(...) {};

extern uint32_t camera_log_level;

#define CAMERA_GET_LOG_LEVEL()                               \
  ({                                                       \
    char prop[PROPERTY_VALUE_MAX];                         \
    property_get("persist.iot-media.camera.log.level", prop, "0"); \
    camera_log_level = atoi(prop);                           \
  })

#define CAMERA_DEBUG(fmt, args...) ALOGD_IF((camera_log_level > 0), fmt, ##args)
#define CAMERA_VERBOSE(fmt, args...) ALOGV_IF((camera_log_level > 1), fmt, ##args)

#ifdef LOG_LEVEL_KPI
#include <cutils/trace.h>

#define BASE_KPI_FLAG   1
#define DETAIL_KPI_FLAG 2

extern volatile uint32_t kpi_debug_level;

#define CAMERA_KPI_GET_MASK() ({\
char prop[PROPERTY_VALUE_MAX];\
property_get("persist.iot-media.kpi.debug", prop, std::to_string(BASE_KPI_FLAG).c_str()); \
kpi_debug_level = atoi (prop);})

class BaseKpiObject {
public:
    BaseKpiObject(const char* str) {
        if (kpi_debug_level >= BASE_KPI_FLAG) {
            atrace_begin(ATRACE_TAG_ALWAYS, str);
        }
    }

    ~BaseKpiObject() {
        if (kpi_debug_level >= BASE_KPI_FLAG) {
            atrace_end(ATRACE_TAG_ALWAYS);
        }
    }
};

#define CAMERA_KPI_BASE() ({\
BaseKpiObject a(__func__);\
})

#define CAMERA_KPI_ASYNC_BEGIN(name, cookie) ({\
if (kpi_debug_level >= BASE_KPI_FLAG) { \
     atrace_async_begin(ATRACE_TAG_ALWAYS, name, cookie); \
}\
})

#define CAMERA_KPI_ASYNC_END(name, cookie) ({\
if (kpi_debug_level >= BASE_KPI_FLAG) { \
     atrace_async_end(ATRACE_TAG_ALWAYS, name, cookie); \
}\
})

class DetailKpiObject {
public:
    DetailKpiObject(const char* str) {
        if (kpi_debug_level >= DETAIL_KPI_FLAG) {
            atrace_begin(ATRACE_TAG_ALWAYS, str);
        }
    }
    ~DetailKpiObject() {
        if (kpi_debug_level >= DETAIL_KPI_FLAG) {
            atrace_end(ATRACE_TAG_ALWAYS);
        }
    }
};

#define CAMERA_KPI_DETAIL() ({\
DetailKpiObject a(__func__);\
})

#else
#define CAMERA_KPI_GET_MASK   ()             do {} while (0)
#define CAMERA_KPI_BASE       ()             do {} while (0)
#define CAMERA_KPI_DETAIL     ()             do {} while (0)
#define CAMERA_KPI_ASYNC_BEGIN(name, cookie) do {} while (0)
#define CAMERA_KPI_ASYNC_END  (name, cookie) do {} while (0)
#endif
