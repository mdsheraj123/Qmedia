LOCAL_PATH := $(call my-dir)

ifeq ($(AI_DIRECTOR),true)

include $(CLEAR_VARS)
LOCAL_MODULE := libSNPE
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MULTILIB := both
LOCAL_SRC_FILES_arm64 := $(SNPE_SDK)/lib/aarch64-android-clang6.0/libSNPE.so
LOCAL_SRC_FILES_arm := $(SNPE_SDK)/lib/arm-android-clang6.0/libSNPE.so
include $(BUILD_PREBUILT)

endif