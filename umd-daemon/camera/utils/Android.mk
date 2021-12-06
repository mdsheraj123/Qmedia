LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := camera_condition.cc
LOCAL_SRC_FILES += camera_thread.cc

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_SHARED_LIBRARIES := \
    libutils \
    liblog

LOCAL_CPPFLAGS += -fexceptions

LOCAL_MODULE = libcamera_utils
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)