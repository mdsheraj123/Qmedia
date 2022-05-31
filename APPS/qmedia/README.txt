# Follow these instructions to build and run SNPE with DSP via AOSP.

git apply SnpeOnAosp.patch

Paste the following files in current directory.

libs/snpe-release.aar (tested with snpe-1.51.0.3413)
libc++_shared.so (from extracting snpe-release.aar)
libSNPE.so (from extracting snpe-release.aar)
libsnpe-android.so (from extracting snpe-release.aar)
libsnpe_dsp_domains_v2.so (from extracting snpe-release.aar)
libsnpe_dsp_domains_v3.so (from extracting snpe-release.aar)


After flashing full build, run

adb wait-for-device
adb root
adb remount
adb disable-verity
adb reboot

adb root
adb remount
adb push qseg_person_align_1_quant.dlc /storage/emulated/0/DCIM/SnpeModel/qseg_person_align_1_quant.dlc
adb push libsnpe_dsp_v66_domains_v2_skel.so /vendor/lib/rfsa/adsp/libsnpe_dsp_v66_domains_v2_skel.so
adb reboot

adb wait-for-device
adb root
adb remount
adb shell setenforce 0

