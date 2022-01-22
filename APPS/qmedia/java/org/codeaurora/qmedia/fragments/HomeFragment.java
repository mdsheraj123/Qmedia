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

import android.app.AlertDialog;
import android.content.Context;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.hardware.display.DisplayManager;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import org.codeaurora.qmedia.CameraBase;
import org.codeaurora.qmedia.CameraDisconnectedListener;
import org.codeaurora.qmedia.HDMIinAudioPlayback;
import org.codeaurora.qmedia.MediaCodecDecoder;
import org.codeaurora.qmedia.MediaCodecRecorder;
import org.codeaurora.qmedia.PresentationBase;
import org.codeaurora.qmedia.R;
import org.codeaurora.qmedia.SettingsUtil;
import org.codeaurora.qmedia.opengles.VideoComposer;

import java.io.File;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;

public class HomeFragment extends Fragment implements CameraDisconnectedListener {

    private static final String TAG = "HomeFragment";
    private static final String FILES_PATH = "/storage/emulated/0/DCIM/Test";

    private final ArrayList<SurfaceView> mSurfaceViewList = new ArrayList<>();
    private final ArrayList<MediaCodecDecoder> mMediaCodecDecoderList = new ArrayList<>();
    private final ArrayList<PresentationBase> mPresentationBaseList = new ArrayList<>();
    private SettingsUtil mSettingData;
    private ArrayList<String> mFileNames;
    private Boolean mPrimaryDisplayStarted = false;
    private int mSurfaceCount = 0;

    private DisplayManager mDisplayManager;
    private Button mPrimaryDisplayButton;
    private CameraBase mCameraBase = null;
    private MediaCodecRecorder mMediaCodecRecorder = null;
    private Boolean mRecorderStarted = false;
    private SurfaceHolder mHDMIinSurfaceHolder;

    private HDMIinAudioPlayback mHDMIinAudioPlayback = null;
    private static final CameraCharacteristics.Key<String> CAMERA_TYPE_CHARACTERISTIC_KEY =
            new CameraCharacteristics.Key<>("camera.type", String.class);
    private Boolean mHDMIinAvailable = false;
    private Boolean mCameraRunningStateSelected = false;
    private Context mContext;
    private final AtomicBoolean mCameraRunning = new AtomicBoolean(false);
    private CameraDisconnectedListener mCameraDisconnectedListenerObject;
    private HandlerThread mAvailabilityCallbackThread;
    private Handler mAvailabilityCallbackHandler;
    private String mHDMIinCameraID = "";

    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        Log.v(TAG, "onCreateView");
        mContext = requireContext();
        mCameraDisconnectedListenerObject = this;
        startAvailabilityCallbackThread();
        // Inflate the layout for this fragment
        mSettingData = new SettingsUtil(getActivity().getApplicationContext());
        mSettingData.printSettingsValues();
        mDisplayManager = (DisplayManager) getActivity().getSystemService(Context.DISPLAY_SERVICE);

        // This code is to handle Concurrent Decode functionality
        if (mSettingData.getHDMISource(0).equals("MP4")) {
            if (mSettingData.getComposeType(0).equals("OpenGLES")) {
                return inflater.inflate(R.layout.opengl_composition, container, false);
            } else if (mSettingData.getComposeType(0).equals("OpenGLESWithEncode")) {
                return inflater.inflate(R.layout.opengles_with_encode, container, false);
            } else {
                switch (mSettingData.getDecoderInstanceNumber(0)) {
                    case 1:
                        return inflater.inflate(R.layout.decode_one, container, false);
                    case 4:
                        return inflater.inflate(R.layout.decode_four, container, false);
                    case 8:
                        return inflater.inflate(R.layout.decode_eight, container, false);
                    case 15:
                        return inflater.inflate(R.layout.decode_fifteen, container, false);
                    case 24:
                        return inflater.inflate(R.layout.decode_max, container, false);
                    default:
                        Log.e(TAG, "Invalid decode configuration");
                        Toast.makeText(getActivity().getApplicationContext(),
                                "Invalid Decode configuration", Toast.LENGTH_SHORT).show();
                }
            }
        }

        // Load Default layout for all other scenario e.g. HDMI In and Concurrent HDMI
        return inflater.inflate(R.layout.primary_display, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
        Log.v(TAG, "onViewCreated enter");
        super.onViewCreated(view, savedInstanceState);
        if (mSettingData.getHDMISource(0).equals("MP4")) {
            loadFileNames();
            handleDecode(view);
        } else if (mSettingData.getHDMISource(0).equals("Camera")) {
            handleCamera(view);
        } else {
            if (mSettingData.getHDMISource(1).equals("None") &&
                    mSettingData.getHDMISource(2).equals("None")) {
                Toast.makeText(getContext(), "Please select at least one source",
                        Toast.LENGTH_SHORT).show();
                return;
            }
            mPrimaryDisplayButton = view.findViewById(R.id.primary_display_button);
            mPrimaryDisplayButton.setOnClickListener((View v) -> processSecondaryDisplaysToggle());
        }
        Log.v(TAG, "onViewCreated exit");
    }

    @Override
    public void onResume() {
        Log.v(TAG, "Enter onResume");
        super.onResume();
        Display[] displays = mDisplayManager.getDisplays(
                DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
        Log.i(TAG, "Number of display # " + displays.length);

        for (int it = 0; it < displays.length; it++) {
            mPresentationBaseList.add(new PresentationBase(getContext(), displays[it],
                    mSettingData, it + 1, requireActivity()));
        }

        for (PresentationBase it : mPresentationBaseList) {
            try {
                it.show();
            } catch (WindowManager.InvalidDisplayException exception) {
                mPresentationBaseList.remove(it);
            }
        }
        Log.v(TAG, "Exit onResume");
    }

    @Override
    public void onPause() {
        Log.v(TAG, "Enter OnPause");
        super.onPause();
        if (mPrimaryDisplayStarted) {
            mCameraRunningStateSelected = false;
            for (PresentationBase it : mPresentationBaseList) {
                it.stop();
            }

            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.stop();
            }
            if (mSettingData.getHDMISource(0).equals("MP4") &&
                    mSettingData.getComposeType(0).equals("OpenGLESWithEncode")) {
                if (mMediaCodecRecorder != null && mRecorderStarted) {
                    mMediaCodecRecorder.stop();
                    mRecorderStarted = false;
                }
            }
            mPrimaryDisplayStarted = false;
            mPrimaryDisplayButton.setText("Start");

            // This will handle primary screen camera op
            if (mCameraBase != null && mCameraRunning.getAndSet(false)) {
                if (mSettingData.getHDMISource(0).equals("Camera") &&
                        mSettingData.getIsHDMIinCameraEnabled(0)) {
                    if (mMediaCodecRecorder != null && mRecorderStarted) {
                        mMediaCodecRecorder.stop();
                        mRecorderStarted = false;
                    }
                    if (mHDMIinAudioPlayback != null) {
                        mHDMIinAudioPlayback.stop();
                        mHDMIinAudioPlayback = null;
                    }
                }
                mCameraBase.stopCamera();
            }
        }
        for (PresentationBase it : mPresentationBaseList) {
            it.dismiss();
        }
        mPresentationBaseList.clear();
        Log.v(TAG, "Exit OnPause");
    }

    private void processDecodeAndSecondaryDisplaysToggle() {
        Log.v(TAG, "processDecodeAndSecondaryDisplaysToggle enter");
        mPrimaryDisplayStarted = !mPrimaryDisplayStarted;
        if (mPrimaryDisplayStarted) {

            if (mSettingData.getComposeType(0).equals("OpenGLESWithEncode")) {
                mRecorderStarted = true;
                mMediaCodecRecorder.start(0);
            }
            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.start();
            }

            for (PresentationBase it : mPresentationBaseList) {
                it.start();
            }
            mPrimaryDisplayButton.setText("Stop");
        } else {
            if (mSettingData.getComposeType(0).equals("OpenGLESWithEncode") && mRecorderStarted) {
                mRecorderStarted = false;
                mMediaCodecRecorder.stop();
            }
            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.stop();
            }
            for (PresentationBase it : mPresentationBaseList) {
                it.stop();
            }
            mPrimaryDisplayButton.setText("Start");
        }
        Log.v(TAG, "processDecodeAndSecondaryDisplaysToggle exit");
    }

    private void processCameraAndSecondaryDisplaysToggle() {
        Log.v(TAG, "processCameraAndSecondaryDisplaysToggle enter");
        mPrimaryDisplayStarted = !mPrimaryDisplayStarted;
        if (mSettingData.getIsHDMIinCameraEnabled(0) && !mHDMIinAvailable) {
            if (mPrimaryDisplayStarted) {
                mCameraRunningStateSelected = true;
                for (PresentationBase it : mPresentationBaseList) {
                    it.start();
                }
                mPrimaryDisplayButton.setText("Stop");
                AlertDialog.Builder alert = new AlertDialog.Builder(mContext);
                alert.setTitle("No device is connected");
                alert.setMessage("To project content connect a device via HDMI.");
                alert.setPositiveButton("OK", null);
                alert.show();
            } else {
                mCameraRunningStateSelected = false;
                for (PresentationBase it : mPresentationBaseList) {
                    it.stop();
                }
                mPrimaryDisplayButton.setText("Start");
            }
        } else {
            if (mCameraBase != null) {
                if (mPrimaryDisplayStarted) {
                    mCameraRunningStateSelected = true;
                    if (!mCameraRunning.getAndSet(true)) {
                        mCameraBase.startCamera(mSettingData.getCameraID(0));
                        if (mSettingData.getIsHDMIinCameraEnabled(0)) {
                            mMediaCodecRecorder.start(0);
                            mRecorderStarted = true;
                            if (mHDMIinAudioPlayback != null) {
                                mHDMIinAudioPlayback.start();
                            }
                        }
                    }
                    for (PresentationBase it : mPresentationBaseList) {
                        it.start();
                    }
                    mPrimaryDisplayButton.setText("Stop");
                } else {
                    mCameraRunningStateSelected = false;
                    if (mCameraRunning.getAndSet(false)) {
                        if (mSettingData.getIsHDMIinCameraEnabled(0)) {
                            if (mMediaCodecRecorder != null && mRecorderStarted) {
                                mMediaCodecRecorder.stop();
                                mRecorderStarted = false;
                            }
                            if (mHDMIinAudioPlayback != null) {
                                mHDMIinAudioPlayback.stop();
                            }
                        }
                        mCameraBase.stopCamera();
                    }
                    for (PresentationBase it : mPresentationBaseList) {
                        it.stop();
                    }
                    mPrimaryDisplayButton.setText("Start");
                }
            }
        }
        Log.v(TAG, "processCameraAndSecondaryDisplaysToggle exit");
    }


    private final CameraManager.AvailabilityCallback mAvailabilityCallback = new CameraManager.AvailabilityCallback() {
        @Override
        public void onCameraAvailable(@NonNull String cameraId) {
            super.onCameraAvailable(cameraId);
            Log.v(TAG, "onCameraAvailable called with cameraId " + cameraId);
            CameraManager manager =
                    (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
            try {
                CameraCharacteristics characteristics = manager.getCameraCharacteristics(cameraId);
                String cameraType = characteristics.get(CAMERA_TYPE_CHARACTERISTIC_KEY);
                if (cameraType != null && cameraType.equals("screen_share_internal")) {
                    Log.i(TAG, "cameraId " + cameraId + " is screen_share_internal");
                    if (!mCameraRunning.get()) {
                        mHDMIinCameraID = cameraId;
                        mMediaCodecRecorder = null;
                        mHDMIinAudioPlayback = null;
                        try {
                            StreamConfigurationMap cameraConfig = characteristics.get(
                                    CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                            Size[] resolution = cameraConfig.getOutputSizes(MediaRecorder.class);
                            for (Size s : resolution) {
                                Log.i(TAG, "HDMIin supported dynamic resolution " +
                                        s.getWidth() + "x" + s.getHeight());
                            }
                            Log.i(TAG, "HDMIin dynamic resolution selected as " +
                                    resolution[0].getWidth() + "x" + resolution[0].getHeight());
                            requireActivity().runOnUiThread(() -> {
                                mHDMIinSurfaceHolder.setFixedSize(resolution[0].getWidth(), resolution[0].getHeight());
                            });
                            mMediaCodecRecorder = new MediaCodecRecorder(mContext, resolution[0].getWidth(),
                                    resolution[0].getHeight(), mSettingData.getIsHDMIinAudioEnabled(0));
                            mCameraBase.addRecorderStream(mMediaCodecRecorder.getRecorderSurface());
                            if (mSettingData.getIsHDMIinAudioEnabled(0)) {
                                mHDMIinAudioPlayback = new HDMIinAudioPlayback(requireContext());
                            }
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        if (mCameraRunningStateSelected && !mCameraRunning.getAndSet(true)) {
                            Log.d(TAG, "onCameraAvailable " +
                                    "mCameraRunningStateSelected and !mCameraRunning so will start");
                            mCameraBase.startCamera(mSettingData.getCameraID(0));
                            mMediaCodecRecorder.start(0);
                            mRecorderStarted = true;
                            if (mHDMIinAudioPlayback != null) {
                                mHDMIinAudioPlayback.start();
                            }
                        }
                        mHDMIinAvailable = true;
                    }
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
            Log.v(TAG, "onCameraAvailable exit");
        }

        @Override
        public void onCameraUnavailable(@NonNull String cameraId) {
            super.onCameraUnavailable(cameraId);
            Log.v(TAG, "onCameraUnavailable called with cameraId " + cameraId);
            if (mHDMIinCameraID.equals(cameraId)) {
                if (!mCameraRunning.get()) {
                    mHDMIinAvailable = false;
                    requireActivity().runOnUiThread(() -> {
                        AlertDialog.Builder alert = new AlertDialog.Builder(mContext);
                        alert.setTitle("No device is connected");
                        alert.setMessage("To project content connect a device via HDMI.");
                        alert.setPositiveButton("OK", null);
                        alert.show();
                    });
                }
            }
            Log.v(TAG, "onCameraUnavailable exit");
        }
    };


    @Override
    public void cameraDisconnected() {
        Thread mCameraDisconnectedThread = new CameraDisconnectedThread();
        mCameraDisconnectedThread.start();
    }

    class CameraDisconnectedThread extends Thread {
        //method where the thread execution will start
        public void run() {
            Log.v(TAG, "CameraDisconnectedThread enter");
            if (mCameraRunning.getAndSet(false)) {
                mHDMIinAvailable = false;
                if (mMediaCodecRecorder != null && mRecorderStarted) {
                    mMediaCodecRecorder.stop();
                    mRecorderStarted = false;
                }
                mMediaCodecRecorder = null;
                if (mHDMIinAudioPlayback != null) {
                    mHDMIinAudioPlayback.stop();
                }
                mHDMIinAudioPlayback = null;
                mCameraBase.stopCamera();
                requireActivity().runOnUiThread(() -> {
                    AlertDialog.Builder alert = new AlertDialog.Builder(mContext);
                    alert.setTitle("No device is connected");
                    alert.setMessage("To project content connect a device via HDMI.");
                    alert.setPositiveButton("OK", null);
                    alert.show();
                });
            }
            Log.v(TAG, "CameraDisconnectedThread exit");
        }
    }

    private void handleCamera(View view) {
        Log.v(TAG, "handleCamera enter");
        mPrimaryDisplayButton = view.findViewById(R.id.primary_display_button);
        mPrimaryDisplayButton
                .setOnClickListener((View v) -> processCameraAndSecondaryDisplaysToggle());
        SurfaceView mPrimaryDisplaySurfaceView = view.findViewById(R.id.primary_surface_view);
        mPrimaryDisplaySurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.v(TAG, "surfaceCreated  for Camera");
                mPrimaryDisplayButton.setEnabled(true);
                mCameraBase = new CameraBase(getContext(), mCameraDisconnectedListenerObject);
                mCameraBase.addPreviewStream(holder);
                if (mSettingData.getIsHDMIinCameraEnabled(0)) {
                    mHDMIinSurfaceHolder = holder;
                    CameraManager manager =
                            (CameraManager) requireContext().getSystemService(Context.CAMERA_SERVICE);
                    manager.registerAvailabilityCallback(mAvailabilityCallback, mAvailabilityCallbackHandler);
                } else {
                    holder.setFixedSize(1920, 1080);
                }
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
            }
        });
        Log.v(TAG, "handleCamera exit");
    }

    private void handleDecode(View view) {
        Log.v(TAG, "handleDecode enter");
        mPrimaryDisplayButton = view.findViewById(R.id.button_decode);
        mPrimaryDisplayButton
                .setOnClickListener((View v) -> processDecodeAndSecondaryDisplaysToggle());
        if (mSettingData.getComposeType(0).equals("OpenGLESWithEncode")) {
            Log.d(TAG, "OpenGLES with Encode is selected");
            mMediaCodecRecorder = new MediaCodecRecorder(getContext(), 1920, 1080, false);
            createMediaCodecDecoderInstances();
        } else {
            createSurfaceView(view);
        }
        Log.v(TAG, "handleDecode exit");
    }

    private void createSurfaceView(View view) {
        Log.v(TAG, "createSurfaceView enter");
        if (mSettingData.getComposeType(0).equals("OpenGLES")) {
            mSurfaceViewList.add(view.findViewById(R.id.primary_surface_view));
        } else {
            switch (mSettingData.getDecoderInstanceNumber(0)) {
                case 1:
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView0));
                    break;
                case 4:
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView0));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView1));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView2));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView3));
                    break;
                case 8:
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView0));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView1));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView2));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView3));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView4));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView5));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView6));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView7));
                    break;
                case 15:
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView0));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView1));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView2));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView3));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView4));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView5));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView6));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView7));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView8));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView9));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView10));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView11));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView12));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView13));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView14));
                    break;
                case 24:
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView0));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView1));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView2));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView3));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView4));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView5));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView6));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView7));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView8));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView9));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView10));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView11));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView12));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView13));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView14));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView15));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView16));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView17));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView18));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView19));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView20));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView21));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView22));
                    mSurfaceViewList.add(view.findViewById(R.id.surfaceView23));
                    break;
            }
        }

        for (SurfaceView surface : mSurfaceViewList) {
            surface.getHolder().addCallback(new SurfaceHolder.Callback() {
                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    updateSurfaceCreatedCount();
                }

                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width,
                                           int height) {
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                }
            });
        }
        Log.v(TAG, "createSurfaceView exit");
    }

    private void updateSurfaceCreatedCount() {
        mSurfaceCount++;
        if (mSurfaceCount == mSurfaceViewList.size()) {
            createMediaCodecDecoderInstances();
        }
    }

    private void createMediaCodecDecoderInstances() {
        Log.v(TAG, "createMediaCodecDecoderInstances enter");
        int count = 0;
        if (mSettingData.getComposeType(0).equals("SF")) {
            for (SurfaceView it : mSurfaceViewList) {
                mMediaCodecDecoderList.add(new MediaCodecDecoder(it.getHolder(),
                        it.getHolder().getSurface(),
                        mFileNames.get(count % mFileNames.size())));
                count++;
            }
        } else {
            VideoComposer mVideoComposer;
            if (mSettingData.getComposeType(0).equals("OpenGLES")) {
                mVideoComposer = new VideoComposer(mSurfaceViewList.get(0).getHolder().getSurface(),
                        1920, 1080, 30f, 0.0f, mSettingData.getDecoderInstanceNumber(0));
            } else { // This is OPENGL With Encode
                mVideoComposer = new VideoComposer(mMediaCodecRecorder.getRecorderSurface(), 1920,
                        1080, 30.0f, 0.0f, mSettingData.getDecoderInstanceNumber(0));
            }
            for (int it = 0; it < mSettingData.getDecoderInstanceNumber(0); it++) {
                mMediaCodecDecoderList.add(new MediaCodecDecoder(null,
                        mVideoComposer.getInputSurface(it),
                        mFileNames.get(count % mFileNames.size())));
                count++;
            }
        }
        Log.v(TAG, "createMediaCodecDecoderInstances exit");
    }

    private void loadFileNames() {
        Log.v(TAG, "loadFileNames enter");
        mFileNames = new ArrayList<>();
        try {
            File[] files = new File(FILES_PATH).listFiles();
            for (File file : files) {
                if (file.isFile()) {
                    Log.i(TAG, "File Names: " + file.getAbsolutePath());
                    mFileNames.add(file.getAbsolutePath());
                }
            }
        } catch (NullPointerException e) {
            Toast.makeText(getContext(), "Please provide the correct path for video files",
                    Toast.LENGTH_SHORT).show();
            e.printStackTrace();
        }
        Log.v(TAG, "loadFileNames exit");
    }


    private void processSecondaryDisplaysToggle() {
        Log.v(TAG, "processSecondaryDisplaysToggle enter");
        mPrimaryDisplayStarted = !mPrimaryDisplayStarted;
        if (mPrimaryDisplayStarted) {
            for (PresentationBase it : mPresentationBaseList) {
                it.start();
            }
            mPrimaryDisplayButton.setText("Stop");
        } else {
            for (PresentationBase it : mPresentationBaseList) {
                it.stop();
            }
            mPrimaryDisplayButton.setText("Start");
        }
        Log.v(TAG, "processSecondaryDisplaysToggle exit");
    }

    private void startAvailabilityCallbackThread() {
        Log.v(TAG, "startAvailabilityCallbackThread enter");
        mAvailabilityCallbackThread = new HandlerThread("AvailabilityCallbackThread");
        mAvailabilityCallbackThread.start();
        mAvailabilityCallbackHandler = new Handler(mAvailabilityCallbackThread.getLooper());
        Log.v(TAG, "startAvailabilityCallbackThread enter");
    }

    private void stopAvailabilityCallbackThread() {
        Log.v(TAG, "stopAvailabilityCallbackThread enter");
        mAvailabilityCallbackThread.quitSafely();
        try {
            mAvailabilityCallbackThread.join();
            mAvailabilityCallbackThread = null;
            mAvailabilityCallbackHandler = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        Log.v(TAG, "stopAvailabilityCallbackThread exit");
    }

    @Override
    public void onDestroy() {
        Log.v(TAG, "onDestroy enter");
        super.onDestroy();
        CameraManager manager =
                (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
        manager.unregisterAvailabilityCallback(mAvailabilityCallback);
        stopAvailabilityCallbackThread();
        Log.v(TAG, "onDestroy exit");
    }
}
