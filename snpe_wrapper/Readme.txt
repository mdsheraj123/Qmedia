Refer to the following document to get SNPE SDK: https://developer.qualcomm.com/sites/default/files/docs/snpe/index.html

Enabling AI Director and SNPE wrapper

Refer to Main Release Notes for directory structure and Chip code

1. In the files: "Android.mk" & "iot-media-system.mk" present in the path: "<workspace>/vendor/qcom/opensource/iot-media", uncomment below line:

# AI_DIRECTOR := true

Note: In <workspace>/vendor/qcom/opensource/iot-media/snpe_wrapper/Android.mk, set the SNPE_SDK variable to the snpe version used.
      Below is the default value set.

"SNPE_SDK := snpe-1.55.0.2958"

2. Extract SNPE SDK archive in <workspace>/vendor/qcom/opensource/iot-media/snpe_wrapper/snpe_lib/

├── snpe_lib
│   ├── Android.mk
│   └── snpe-1.55.0.2958

3. Once changes are made, build Android images as usual.

4. After first boot perform following steps in order to enable AI Director & SNPE
  adb root
  adb disable-verity
  adb reboot
  adb wait-for-device
  adb root
  adb remount

  Upload SNPE related libraries
  adb push snpe-1.55.0.2958/lib/aarch64-android-clang6.0/* /vendor/lib64/
  adb push snpe-1.55.0.2958/lib/dsp/* /vendor/lib/rfsa/adsp/

  Upload SNPE model
  adb shell mkdir -p /vendor/etc/camera
  adb push yolov5s_relu_finetune_quantized_cle_bc.dlc /vendor/etc/camera/

  adb shell "echo outputFormat=0 >> /vendor/etc/camera/camxoverridesettings.txt"
  adb shell "echo enableAIDirector=TRUE >> /vendor/etc/camera/camxoverridesettings.txt"
  adb reboot