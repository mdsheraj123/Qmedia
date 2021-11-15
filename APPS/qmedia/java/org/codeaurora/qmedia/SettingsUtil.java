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
*/

package org.codeaurora.qmedia;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import androidx.preference.PreferenceManager;

import java.util.ArrayList;

class SettingsData {

    protected String src;
    protected int decodeInstance;
    protected String composeType;
    protected String camID;
}

public class SettingsUtil {

    private static final String TAG = "SettingsUtil";

    public ArrayList<SettingsData> data;

    public SettingsUtil(Context context) {
        data = new ArrayList<>();
        SharedPreferences pref = PreferenceManager.getDefaultSharedPreferences(context);

        SettingsData hdmi_1_setting = new SettingsData();
        hdmi_1_setting.src = pref.getString("hdmi_1_source", "None");
        hdmi_1_setting.decodeInstance =
                Integer.parseInt(pref.getString("hdmi_1_decoder_instance", "1"));
        hdmi_1_setting.composeType = pref.getString("hdmi_1_compose_view", "SF");
        hdmi_1_setting.camID = pref.getString("hdmi_1_camera_id", "0");
        data.add(hdmi_1_setting);

        SettingsData hdmi_2_setting = new SettingsData();
        hdmi_2_setting.src = pref.getString("hdmi_2_source", "None");
        hdmi_2_setting.decodeInstance =
                Integer.parseInt(pref.getString("hdmi_2_decoder_instance", "1"));
        hdmi_2_setting.composeType = pref.getString("hdmi_2_compose_view", "SF");
        hdmi_2_setting.camID = pref.getString("hdmi_2_camera_id", "0");
        data.add(hdmi_2_setting);

        SettingsData hdmi_3_setting = new SettingsData();
        hdmi_3_setting.src = pref.getString("hdmi_3_source", "None");
        hdmi_3_setting.decodeInstance =
                Integer.parseInt(pref.getString("hdmi_3_decoder_instance", "1"));
        hdmi_3_setting.composeType = pref.getString("hdmi_3_compose_view", "SF");
        hdmi_3_setting.camID = pref.getString("hdmi_3_camera_id", "0");
        data.add(hdmi_3_setting);
    }

    public void printSettingsValues() {
        for (int it = 0; it < data.size(); it++) {
            Log.d(TAG, "Source " + it + ":" + data.get(it).src);
            Log.d(TAG, "Decoder Instance : " + data.get(it).decodeInstance);
            Log.d(TAG, "Compose Type : " + data.get(it).composeType);
            Log.d(TAG, "Camera ID : " + data.get(it).camID);
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
}
