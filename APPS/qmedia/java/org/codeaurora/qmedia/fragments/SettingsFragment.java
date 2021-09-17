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

package org.codeaurora.qmedia.fragments;

import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.View;
import android.widget.Toast;

import androidx.preference.EditTextPreference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.preference.SwitchPreference;

import org.codeaurora.qmedia.R;

public class SettingsFragment extends PreferenceFragmentCompat
        implements SharedPreferences.OnSharedPreferenceChangeListener {
    private PreferenceScreen mPrefScreen;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.setting_preference, rootKey);
        mPrefScreen = this.getPreferenceScreen();

        try {
            EditTextPreference version_info =
                    (EditTextPreference) mPrefScreen.findPreference("version_info");
            version_info.setSummary(getActivity().getApplicationContext().getPackageManager().
                    getPackageInfo(getActivity().getApplicationContext().getPackageName(),
                            0).versionName);
            version_info.setEnabled(false);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        int navHeight = getResources().getDimensionPixelSize(getResources().
                getIdentifier("navigation_bar_height", "dimen", "android"));
        if (navHeight > 0) {
            view.setPadding(0, 0, 0, navHeight);
        }

    }

    @Override
    public void onResume() {
        super.onResume();
        mPrefScreen.getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onPause() {
        super.onPause();
        mPrefScreen.getSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {

        SwitchPreference hdmi = (SwitchPreference) mPrefScreen.findPreference("hdmi_enable");
        SwitchPreference decode =
                (SwitchPreference) mPrefScreen.findPreference("concurrent_decode_enable");
        SwitchPreference hdmi_in = (SwitchPreference) mPrefScreen.findPreference("hdmi_in_enable");

        switch (key) {
            case "hdmi_enable": {
                boolean checked = hdmi.isChecked();
                if (checked) {
                    decode.setChecked(false);
                    hdmi_in.setChecked(false);
                }
            }
            case "concurrent_decode_enable": {
                boolean checked = decode.isChecked();
                if (checked) {
                    hdmi.setChecked(false);
                    hdmi_in.setChecked(false);
                }
            }
            case "hdmi_in_enable": {
                boolean checked = hdmi_in.isChecked();
                if (checked) {
                    decode.setChecked(false);
                    hdmi.setChecked(false);
                }
            }

        }

        if (!(hdmi.isChecked() || decode.isChecked() || hdmi_in.isChecked())) {
            Toast.makeText(getContext().getApplicationContext(),
                    "Please select at least one functionality", Toast.LENGTH_SHORT).show();
        }
    }
}