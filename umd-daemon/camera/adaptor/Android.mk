LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE = libcamera_adaptor
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CPPFLAGS += -fexceptions
LOCAL_CPPFLAGS += -DQCAMERA3_TAG_LOCAL_COPY

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := camera_device_client.cc
LOCAL_SRC_FILES += camera_monitor.cc
LOCAL_SRC_FILES += camera_request_handler.cc
LOCAL_SRC_FILES += camera_prepare_handler.cc
LOCAL_SRC_FILES += camera_stream.cc
LOCAL_SRC_FILES += camera_utils.cc
LOCAL_SRC_FILES += camera_hidl_vendor_tag_descriptor.cc

LOCAL_C_INCLUDES += $(LOCAL_PATH)/..
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/..
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)/../include

LOCAL_SHARED_LIBRARIES := \
    libhidlbase \
    libhwbinder \
    libui \
    libbinder \
    libcamera_metadata \
    libutils \
    libcutils \
    libfmq \
    liblog \
    libc++ \
    libcamera_utils \
    libcamera_memory_interface \
    android.hardware.camera.provider@2.4 \
    android.hardware.camera.device@1.0 \
    android.hardware.camera.device@3.2 \
    android.hardware.camera.metadata@3.4 \
    android.hardware.camera.common@1.0 \
    android.hardware.graphics.common@1.0 \
    android.hardware.graphics.allocator@3.0 \
    android.hardware.graphics.mapper@3.0

LOCAL_STATIC_LIBRARIES := \
    android.hardware.camera.common@1.0-helper

include $(BUILD_SHARED_LIBRARY)
