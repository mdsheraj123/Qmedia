LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := camera_memory_interface.cc
LOCAL_SRC_FILES += allocator_hidl_interface.cc

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/..

LOCAL_SHARED_LIBRARIES := \
    libcamera_utils \
    libcamera_metadata \
    libhidlbase \
    libhwbinder \
    libutils \
    libcutils \
    liblog \
    android.hardware.graphics.allocator@3.0 \
    android.hardware.graphics.mapper@3.0 \
    android.hardware.camera.metadata@3.4 \
    android.hardware.camera.provider@2.4 \
    android.hardware.camera.device@1.0 \
    android.hardware.camera.device@3.2 \
    android.hardware.camera.common@1.0 \
    android.hardware.graphics.common@1.0

LOCAL_STATIC_LIBRARIES := \
    android.hardware.camera.common@1.0-helper


LOCAL_MODULE = libcamera_memory_interface
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)