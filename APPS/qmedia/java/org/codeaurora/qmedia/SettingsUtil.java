/*
# Copyright (c) 2021 The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Changes from Qualcomm Innovation Center are provided under the following license:
# Copyright (c) 2022 Qualcomm Innovation Center, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the
# disclaimer below) provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#
#    * Redistributions in binary form must reproduce the above
#      copyright notice, this list of conditions and the following
#      disclaimer in the documentation and/or other materials provided
#      with the distribution.
#
#    * Neither the name Qualcomm Innovation Center nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
# NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
# GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
# HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package org.codeaurora.qmedia;

import android.content.Context;
import android.content.SharedPreferences;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.util.Log;

import androidx.preference.PreferenceManager;

import java.util.ArrayList;

class SettingsData {

    protected String src;
    protected int decodeInstance;
    protected String composeType;
    protected String camID;
    protected Boolean isHDMIinCameraEnabled;
    protected Boolean isHDMIinAudioEnabled;

}

public class SettingsUtil {

    private static final String TAG = "SettingsUtil";

    public ArrayList<SettingsData> data;
    private static final CameraCharacteristics.Key<String> CAMERA_TYPE_CHARACTERISTIC_KEY =
            new CameraCharacteristics.Key<>("camera.type", String.class);

    public SettingsUtil(Context context) {
        Log.v(TAG, "SettingsUtil enter");
        data = new ArrayList<>();
        SharedPreferences pref = PreferenceManager.getDefaultSharedPreferences(context);
        CameraManager manager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);

        SettingsData hdmi_1_setting = new SettingsData();
        hdmi_1_setting.src = pref.getString("hdmi_1_source", "None");
        hdmi_1_setting.decodeInstance =
                Integer.parseInt(pref.getString("hdmi_1_decoder_instance", "1"));
        hdmi_1_setting.composeType = pref.getString("hdmi_1_compose_view", "SF");
        hdmi_1_setting.camID = pref.getString("hdmi_1_camera_id", "0");

        String cameraType = null;
        try {
            CameraCharacteristics characteristics = manager.getCameraCharacteristics(hdmi_1_setting.camID);
            cameraType = characteristics.get(CAMERA_TYPE_CHARACTERISTIC_KEY);
        } catch (Exception e) {
            e.printStackTrace();
        }
        if (cameraType != null && cameraType.equals("screen_share_internal")) {
            hdmi_1_setting.isHDMIinCameraEnabled = true;
        } else {
            hdmi_1_setting.isHDMIinCameraEnabled = false;
        }
        hdmi_1_setting.isHDMIinAudioEnabled = pref.getBoolean("hdmi_1_hdmi_in_audio_enable", false);

        data.add(hdmi_1_setting);

        SettingsData hdmi_2_setting = new SettingsData();
        hdmi_2_setting.src = pref.getString("hdmi_2_source", "None");
        hdmi_2_setting.decodeInstance =
                Integer.parseInt(pref.getString("hdmi_2_decoder_instance", "1"));
        hdmi_2_setting.composeType = pref.getString("hdmi_2_compose_view", "SF");
        hdmi_2_setting.camID = pref.getString("hdmi_2_camera_id", "0");

        try {
            CameraCharacteristics characteristics = manager.getCameraCharacteristics(hdmi_2_setting.camID);
            cameraType = characteristics.get(CAMERA_TYPE_CHARACTERISTIC_KEY);
        } catch (Exception e) {
            e.printStackTrace();
        }
        if (cameraType != null && cameraType.equals("screen_share_internal")) {
            hdmi_2_setting.isHDMIinCameraEnabled = true;
        } else {
            hdmi_2_setting.isHDMIinCameraEnabled = false;
        }
        hdmi_2_setting.isHDMIinAudioEnabled = pref.getBoolean("hdmi_2_hdmi_in_audio_enable", false);

        data.add(hdmi_2_setting);

        SettingsData hdmi_3_setting = new SettingsData();
        hdmi_3_setting.src = pref.getString("hdmi_3_source", "None");
        hdmi_3_setting.decodeInstance =
                Integer.parseInt(pref.getString("hdmi_3_decoder_instance", "1"));
        hdmi_3_setting.composeType = pref.getString("hdmi_3_compose_view", "SF");
        hdmi_3_setting.camID = pref.getString("hdmi_3_camera_id", "0");

        try {
            CameraCharacteristics characteristics = manager.getCameraCharacteristics(hdmi_3_setting.camID);
            cameraType = characteristics.get(CAMERA_TYPE_CHARACTERISTIC_KEY);
        } catch (Exception e) {
            e.printStackTrace();
        }
        if (cameraType != null && cameraType.equals("screen_share_internal")) {
            hdmi_3_setting.isHDMIinCameraEnabled = true;
        } else {
            hdmi_3_setting.isHDMIinCameraEnabled = false;
        }
        hdmi_3_setting.isHDMIinAudioEnabled = pref.getBoolean("hdmi_3_hdmi_in_audio_enable", false);

        data.add(hdmi_3_setting);
        Log.v(TAG, "SettingsUtil exit");
    }

    public void printSettingsValues() {
        for (int it = 0; it < data.size(); it++) {
            Log.d(TAG, "Source " + it + ":" + data.get(it).src);
            Log.d(TAG, "Decoder Instance : " + data.get(it).decodeInstance);
            Log.d(TAG, "Compose Type : " + data.get(it).composeType);
            Log.d(TAG, "Camera ID : " + data.get(it).camID);
            Log.d(TAG, "Is HDMIin Camera Enabled : " + data.get(it).isHDMIinCameraEnabled);
            Log.d(TAG, "Is HDMIin Audio Enabled : " + data.get(it).isHDMIinAudioEnabled);
            Log.d(TAG, "#####################################");
        }
    }

    public String getHDMISource(int index) {
        return data.get(index).src;
    }

    public int getDecoderInstanceNumber(int index) {
        return data.get(index).decodeInstance;
    }

    public String getComposeType(int index) {
        return data.get(index).composeType;
    }

    public String getCameraID(int index) {
        return data.get(index).camID;
    }

    public Boolean getIsHDMIinCameraEnabled(int index) {
        return data.get(index).isHDMIinCameraEnabled;
    }

    public Boolean getIsHDMIinAudioEnabled(int index) {
        return data.get(index).isHDMIinAudioEnabled;
    }
}
