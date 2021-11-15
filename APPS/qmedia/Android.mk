LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

res_dir := res $(LOCAL_PATH)/res

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, java)

LOCAL_RESOURCE_DIR := $(addprefix $(LOCAL_PATH)/, $(res_dir))
LOCAL_USE_AAPT2 := true

LOCAL_JAVA_LIBRARIES := com.google.android.material_material \

LOCAL_STATIC_ANDROID_LIBRARIES := \
        androidx.appcompat_appcompat \
        androidx-constraintlayout_constraintlayout \
        androidx.preference_preference \
        androidx.fragment_fragment \
        androidx.core_core \

LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_PACKAGE_NAME := QMedia
LOCAL_PRIVATE_PLATFORM_APIS := true

include $(BUILD_PACKAGE)
