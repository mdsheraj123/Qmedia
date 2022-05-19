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

package org.codeaurora.qmedia.fragments;

import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.preference.EditTextPreference;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.preference.SwitchPreference;

import org.codeaurora.qmedia.R;

import java.util.ArrayList;

public class SettingsFragment extends PreferenceFragmentCompat
        implements SharedPreferences.OnSharedPreferenceChangeListener {
    private static final String TAG = "SettingsFragment";
    private PreferenceScreen mPrefScreen;
    private static final CameraCharacteristics.Key<String> CAMERA_TYPE_CHARACTERISTIC_KEY =
            new CameraCharacteristics.Key<>("camera.type", String.class);
    private String mHDMIinCameraID;
    private Context mContext;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mContext = requireContext();
        CameraManager manager =
                (CameraManager) requireContext().getSystemService(Context.CAMERA_SERVICE);
        manager.registerAvailabilityCallback(mAvailabilityCallback, new Handler());

        setPreferencesFromResource(R.xml.setting_preference, rootKey);
        mPrefScreen = this.getPreferenceScreen();

        try {
            EditTextPreference version_info = mPrefScreen.findPreference("version_info");
            version_info.setSummary(getActivity().getApplicationContext().getPackageManager().
                    getPackageInfo(getActivity().getApplicationContext().getPackageName(),
                            0).versionName);
            version_info.setEnabled(false);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
        // Populate camera IDs
        populateCameraIDs();
        // Update Preference
        updatePreference();
    }


    private final CameraManager.AvailabilityCallback mAvailabilityCallback = new CameraManager.AvailabilityCallback() {
        @Override
        public void onCameraAvailable(@NonNull String cameraId) {
            super.onCameraAvailable(cameraId);
            CameraManager manager =
                    (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
            try {
                CameraCharacteristics characteristics = manager.getCameraCharacteristics(cameraId);
                String cameraType = characteristics.get(CAMERA_TYPE_CHARACTERISTIC_KEY);
                if (cameraType != null && cameraType.equals("screen_share_internal")) {
                    Log.i(TAG, "mHDMIinCameraID is " + cameraId);
                    mHDMIinCameraID = cameraId;
                    populateCameraIDs();
                    updatePreference();
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    };

    @Override
    public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
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
        updatePreference();
    }

    void updatePreference() {
        ListPreference hdmi_source = mPrefScreen.findPreference("hdmi_1_source");
        ListPreference decoder_instance = mPrefScreen.findPreference("hdmi_1_decoder_instance");
        ListPreference compose_view = mPrefScreen.findPreference("hdmi_1_compose_view");
        ListPreference camera_id = mPrefScreen.findPreference("hdmi_1_camera_id");
        ListPreference camera_size = mPrefScreen.findPreference("hdmi_1_camera_size");
        SwitchPreference hdmiin_audio_enable = mPrefScreen.findPreference("hdmi_1_hdmi_in_audio_enable");
        SwitchPreference hdmiin_video_enable = mPrefScreen.findPreference("hdmi_1_hdmi_in_video_enable");
        SwitchPreference reproc_enable = mPrefScreen.findPreference("hdmi_1_reproc_enable");

        if (hdmi_source.getValue().equals("MP4")) {
            if (!decoder_instance.isVisible()) {
                decoder_instance.setVisible(true);
            }
            if (!compose_view.isVisible()) {
                compose_view.setVisible(true);
            }
            if (camera_id.isVisible()) {
                camera_id.setVisible(false);
            }
            if (camera_size.isVisible()) {
                camera_size.setVisible(false);
            }
            if (hdmiin_audio_enable.isVisible()) {
                hdmiin_audio_enable.setVisible(false);
            }
            if (hdmiin_video_enable.isVisible()) {
                hdmiin_video_enable.setVisible(false);
            }
            if (reproc_enable.isVisible()) {
                reproc_enable.setVisible(false);
            }
        } else if (hdmi_source.getValue().equals("Camera")) {
            if (decoder_instance.isVisible()) {
                decoder_instance.setVisible(false);
            }
            if (compose_view.isVisible()) {
                compose_view.setVisible(false);
            }
            if (!camera_id.isVisible()) {
                camera_id.setVisible(true);
            }
            if (camera_id.getValue().equals(mHDMIinCameraID)) {
                hdmiin_audio_enable.setVisible(true);
                hdmiin_video_enable.setVisible(true);
                reproc_enable.setVisible(false);
                camera_size.setVisible(false);
            } else {
                hdmiin_audio_enable.setVisible(false);
                hdmiin_video_enable.setVisible(false);
                reproc_enable.setVisible(true);
                camera_size.setVisible(true);
            }
        } else {
            decoder_instance.setVisible(false);
            compose_view.setVisible(false);
            camera_id.setVisible(false);
            camera_size.setVisible(false);
            hdmiin_audio_enable.setVisible(false);
            hdmiin_video_enable.setVisible(false);
            reproc_enable.setVisible(false);
        }

        hdmi_source = mPrefScreen.findPreference("hdmi_2_source");
        decoder_instance = mPrefScreen.findPreference("hdmi_2_decoder_instance");
        compose_view = mPrefScreen.findPreference("hdmi_2_compose_view");
        camera_id = mPrefScreen.findPreference("hdmi_2_camera_id");
        camera_size = mPrefScreen.findPreference("hdmi_2_camera_size");
        hdmiin_audio_enable = mPrefScreen.findPreference("hdmi_2_hdmi_in_audio_enable");
        hdmiin_video_enable = mPrefScreen.findPreference("hdmi_2_hdmi_in_video_enable");
        reproc_enable = mPrefScreen.findPreference("hdmi_2_reproc_enable");

        if (hdmi_source.getValue().equals("MP4")) {
            if (!decoder_instance.isVisible()) {
                decoder_instance.setVisible(true);
            }
            if (!compose_view.isVisible()) {
                compose_view.setVisible(true);
            }
            if (camera_id.isVisible()) {
                camera_id.setVisible(false);
            }
            if (camera_size.isVisible()) {
                camera_size.setVisible(false);
            }
            if (hdmiin_audio_enable.isVisible()) {
                hdmiin_audio_enable.setVisible(false);
            }
            if (hdmiin_video_enable.isVisible()) {
                hdmiin_video_enable.setVisible(false);
            }
            if (reproc_enable.isVisible()) {
                reproc_enable.setVisible(false);
            }
        } else if (hdmi_source.getValue().equals("Camera")) {
            if (decoder_instance.isVisible()) {
                decoder_instance.setVisible(false);
            }
            if (compose_view.isVisible()) {
                compose_view.setVisible(false);
            }
            if (!camera_id.isVisible()) {
                camera_id.setVisible(true);
            }
            if (camera_id.getValue().equals(mHDMIinCameraID)) {
                hdmiin_audio_enable.setVisible(true);
                hdmiin_video_enable.setVisible(true);
                reproc_enable.setVisible(false);
                camera_size.setVisible(false);
            } else {
                hdmiin_audio_enable.setVisible(false);
                hdmiin_video_enable.setVisible(false);
                reproc_enable.setVisible(true);
                camera_size.setVisible(true);
            }
        } else {
            decoder_instance.setVisible(false);
            compose_view.setVisible(false);
            camera_id.setVisible(false);
            hdmiin_audio_enable.setVisible(false);
            hdmiin_video_enable.setVisible(false);
            reproc_enable.setVisible(false);
            camera_size.setVisible(false);
        }

        hdmi_source = mPrefScreen.findPreference("hdmi_3_source");
        decoder_instance = mPrefScreen.findPreference("hdmi_3_decoder_instance");
        compose_view = mPrefScreen.findPreference("hdmi_3_compose_view");
        camera_id = mPrefScreen.findPreference("hdmi_3_camera_id");
        camera_size = mPrefScreen.findPreference("hdmi_3_camera_size");
        hdmiin_audio_enable = mPrefScreen.findPreference("hdmi_3_hdmi_in_audio_enable");
        hdmiin_video_enable = mPrefScreen.findPreference("hdmi_3_hdmi_in_video_enable");
        reproc_enable = mPrefScreen.findPreference("hdmi_3_reproc_enable");

        if (hdmi_source.getValue().equals("MP4")) {
            if (!decoder_instance.isVisible()) {
                decoder_instance.setVisible(true);
            }
            if (!compose_view.isVisible()) {
                compose_view.setVisible(true);
            }
            if (camera_id.isVisible()) {
                camera_id.setVisible(false);
            }
            if (camera_size.isVisible()) {
                camera_size.setVisible(false);
            }
            if (hdmiin_audio_enable.isVisible()) {
                hdmiin_audio_enable.setVisible(false);
            }
            if (hdmiin_video_enable.isVisible()) {
                hdmiin_video_enable.setVisible(false);
            }
            if (reproc_enable.isVisible()) {
                reproc_enable.setVisible(false);
            }
        } else if (hdmi_source.getValue().equals("Camera")) {
            if (decoder_instance.isVisible()) {
                decoder_instance.setVisible(false);
            }
            if (compose_view.isVisible()) {
                compose_view.setVisible(false);
            }
            if (!camera_id.isVisible()) {
                camera_id.setVisible(true);
            }
            if (camera_id.getValue().equals(mHDMIinCameraID)) {
                hdmiin_audio_enable.setVisible(true);
                hdmiin_video_enable.setVisible(true);
                reproc_enable.setVisible(false);
                camera_size.setVisible(false);
            } else {
                hdmiin_audio_enable.setVisible(false);
                hdmiin_video_enable.setVisible(false);
                reproc_enable.setVisible(true);
                camera_size.setVisible(true);
            }
        } else {
            decoder_instance.setVisible(false);
            compose_view.setVisible(false);
            camera_id.setVisible(false);
            hdmiin_audio_enable.setVisible(false);
            hdmiin_video_enable.setVisible(false);
            reproc_enable.setVisible(false);
            camera_size.setVisible(false);
        }

        // Handle Reset App Preference
        Preference button = mPrefScreen.findPreference("reset");
        button.setOnPreferenceClickListener(preference -> {
            DialogInterface.OnClickListener dialogClickListener = (dialog, which) -> {
                switch (which) {
                    case DialogInterface.BUTTON_POSITIVE:
                        //Yes button clicked
                        restoreAppPreference();
                        break;

                    case DialogInterface.BUTTON_NEGATIVE:
                        //No button clicked
                        break;
                }
            };

            AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
            builder.setTitle("Restore App Preference");
            builder.setMessage("Do you wish to continue.").
                    setPositiveButton("Yes", dialogClickListener).
                    setNegativeButton("No", dialogClickListener).
                    show();
            return true;
        });

    }

    String getLensOrientationString(int facing) {
        String out = "Unknown";
        switch (facing) {
            case CameraCharacteristics.LENS_FACING_BACK:
                out = "Back";
                break;
            case CameraCharacteristics.LENS_FACING_FRONT:
                out = "Front";
                break;
            case CameraCharacteristics.LENS_FACING_EXTERNAL:
                out = "External";
                break;
        }
        return out;
    }

    void populateCameraIDs() {
        CameraManager cameraManager =
                (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
        ArrayList<String> detectedCameras = new ArrayList<>();
        ArrayList<String> cameraIDs = new ArrayList<>();
        try {
            for (String camID : cameraManager.getCameraIdList()) {
                if (camID.equals(mHDMIinCameraID)) {
                    detectedCameras.add("HDMIin");
                } else {
                    detectedCameras.add(getLensOrientationString(
                            cameraManager.getCameraCharacteristics(camID)
                                    .get(CameraCharacteristics.LENS_FACING)) + "(" + camID + ")");
                }
                cameraIDs.add(camID);
            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }

        ListPreference camera_id = mPrefScreen.findPreference("hdmi_1_camera_id");
        CharSequence[] cameras = detectedCameras.toArray(new CharSequence[detectedCameras.size()]);
        CharSequence[] cameraIds =
                cameraIDs.toArray(new CharSequence[cameraIDs.size()]);
        camera_id.setEntries(cameras);
        camera_id.setEntryValues(cameraIds);

        camera_id = mPrefScreen.findPreference("hdmi_2_camera_id");
        camera_id.setEntries(cameras);
        camera_id.setEntryValues(cameraIds);

        camera_id = mPrefScreen.findPreference("hdmi_3_camera_id");
        camera_id.setEntries(cameras);
        camera_id.setEntryValues(cameraIds);
    }

    private void restoreAppPreference() {
        SharedPreferences preferences =
                PreferenceManager.getDefaultSharedPreferences(getActivity());
        SharedPreferences.Editor editor = preferences.edit();
        editor.clear();
        editor.apply();
        PreferenceManager.setDefaultValues(getActivity(), R.xml.setting_preference, true);
        getPreferenceScreen().removeAll();
        onCreatePreferences(null, null);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        CameraManager manager =
                (CameraManager) requireContext().getSystemService(Context.CAMERA_SERVICE);
        manager.unregisterAvailabilityCallback(mAvailabilityCallback);
    }
}