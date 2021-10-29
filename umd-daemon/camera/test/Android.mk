LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE = camera_test
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SRC_FILES := camera_test.cpp

LOCAL_SHARED_LIBRARIES := \
    libcamera_adaptor \
    libcamera_memory_interface \
    libcamera_metadata \
    libcamera_utils \
    libutils \
    libcutils \
    liblog \
    libfmq \
    android.hardware.camera.provider@2.4 \
    android.hardware.camera.device@1.0 \
    android.hardware.camera.device@3.2 \
    android.hardware.camera.metadata@3.4 \
    android.hardware.camera.common@1.0 \
    android.hardware.graphics.common@1.0 \
    android.hardware.graphics.allocator@3.0 \
    android.hardware.graphics.mapper@3.0

LOCAL_STATIC_LIBRARIES := \
    android.hardware.camera.common@1.0-helper \

include $(BUILD_EXECUTABLE)

