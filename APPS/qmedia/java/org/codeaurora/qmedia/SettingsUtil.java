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

    protected boolean concurrentHdmiEnable;
    protected ArrayList<String> sources;

    protected boolean decodeEnable;
    protected int decodeInstance;
    protected String composeType;

    protected boolean hdmiInEnable;
}

public class SettingsUtil {

    private static final String TAG = "SettingsUtil";

    public SettingsData data;

    public SettingsUtil(Context context) {
        data = new SettingsData();

        SharedPreferences pref = PreferenceManager.getDefaultSharedPreferences(context);

        data.concurrentHdmiEnable = pref.getBoolean("hdmi_enable", true);
        data.sources = new ArrayList<>();
        data.sources.add(pref.getString("primary_hdmi", "MP4"));
        data.sources.add(pref.getString("second_hdmi", "MP4"));
        data.sources.add(pref.getString("third_hdmi", "MP4"));

        data.decodeEnable = pref.getBoolean("concurrent_decode_enable", false);
        data.decodeInstance = Integer.parseInt(pref.getString("decode_instance", "1"));
        data.composeType = pref.getString("compose_view", "SF");

        data.hdmiInEnable = pref.getBoolean("hdmi_in_enable", false);
    }

    public void printSettingsValues() {
        Log.d(TAG, "Concurrent HDMI : " + data.concurrentHdmiEnable + "\n");
        Log.d(TAG, "Size of sources" + data.sources.size());
        for (int it = 0; it < data.sources.size(); it++) {
            Log.d(TAG, "Source# " + it + ": " + data.sources.get(it));
        }
        Log.d(TAG, "Concurrent Decode : " + data.decodeEnable + "\n");
        Log.d(TAG, "Decode Instance : " + data.decodeInstance + "\n");
        Log.d(TAG, "Compose and View Type : " + data.composeType + "\n");
        Log.d(TAG, "HDMI In : " + data.hdmiInEnable + "\n");
    }

    public boolean getDecodeStatus() {
        return data.decodeEnable;
    }

    public int getDecodeInstance() {
        return data.decodeInstance;
    }

    public String getComposeType() {
        return data.composeType;
    }

    public boolean getConcurrentHDMIStatus() {
        return data.concurrentHdmiEnable;
    }

    public ArrayList<String> getConcurrentHDMISource() {
        return data.sources;
    }

    public boolean getHDMIInStatus() {
        return data.hdmiInEnable;
    }
}
