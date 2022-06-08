IOT_MEDIA_PATH:= $(call my-dir)

include $(IOT_MEDIA_PATH)/APPS/Android.mk

# Enable AI Director test & SNPE Lib
# AI_DIRECTOR := true

ifeq ($(TARGET_BOARD_PLATFORM),kona)
include $(IOT_MEDIA_PATH)/umd-daemon/camera/Android.mk
include $(IOT_MEDIA_PATH)/umd-daemon/daemon/Android.mk
ifeq ($(AI_DIRECTOR), true)
include $(IOT_MEDIA_PATH)/ai-director-test/Android.mk
endif
include $(IOT_MEDIA_PATH)/snpe_wrapper/Android.mk
endif
