LOCAL_PATH := $(call my-dir)

# SNPE_WRAPPER_COMPILATION := true
# SNPE_SDK := snpe-1.55.0.2958

include $(CLEAR_VARS)

LOCAL_MODULE = libai_snpe_wrapper
LOCAL_VENDOR_MODULE := true

LOCAL_CPPFLAGS += -frtti
LOCAL_CPPFLAGS += -fexceptions

LOCAL_CPP_EXTENSION := .cc

LOCAL_SHARED_LIBRARIES := \
  libutils \
  libcutils \
  liblog

ifeq ($(SNPE_WRAPPER_COMPILATION),true)
  LOCAL_SRC_FILES := snpe_wrapper.cc
  LOCAL_C_INCLUDES += $(LOCAL_PATH)/snpe_lib/$(SNPE_SDK)/include/zdl

  LOCAL_SHARED_LIBRARIES += libSNPE
else
  LOCAL_SRC_FILES := snpe_wrapper_dummy.cc
endif

LOCAL_COPY_HEADERS := snpe_wrapper_interface.h

include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/snpe_lib/Android.mk
