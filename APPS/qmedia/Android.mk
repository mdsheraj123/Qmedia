LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

res_dir := res $(LOCAL_PATH)/res

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES += java/org/codeaurora/qmedia/CameraBase.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/CameraDisconnectedListener.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/HDMIinAudioPlayback.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/MainActivity.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/MediaCodecDecoder.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/MediaCodecRecorder.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/PresentationBase.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/SettingsUtil.java

LOCAL_SRC_FILES += java/org/codeaurora/qmedia/fragments/HomeFragment.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/fragments/PermissionFragment.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/fragments/SettingsFragment.java

LOCAL_SRC_FILES += java/org/codeaurora/qmedia/opengles/InputComposerSurface.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/opengles/OutputComposerSurface.java
LOCAL_SRC_FILES += java/org/codeaurora/qmedia/opengles/VideoComposer.java

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
