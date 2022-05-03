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

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Presentation;
import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.view.Display;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.Toast;

import androidx.annotation.NonNull;

import org.codeaurora.qmedia.opengles.VideoComposer;

import java.io.File;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;


public class PresentationBase extends Presentation implements CameraDisconnectedListener {

    private static final String TAG = "PresentationBase";
    private static final String FILES_PATH = "/storage/emulated/0/DCIM/Test";
    private final ArrayList<MediaCodecDecoder> mMediaCodecDecoderList = new ArrayList<>();
    private final ArrayList<SurfaceView> mSurfaceViewList = new ArrayList<>();
    private ImageView mImageView;
    private Button mReprocButton;
    private CameraBase mCameraBase = null;
    private final SettingsUtil mData;
    private final int mPresentationIndex;
    private MediaCodecRecorder mMediaCodecRecorder = null;
    private ArrayList<String> mFileNames;
    private int mSurfaceCount = 0;

    private HDMIinAudioPlayback mHDMIinAudioPlayback = null;
    private SurfaceHolder mHDMIinSurfaceHolder;
    private static final CameraCharacteristics.Key<String> CAMERA_TYPE_CHARACTERISTIC_KEY =
            new CameraCharacteristics.Key<>("camera.type", String.class);
    private Boolean mHDMIinAvailable = false;
    private Boolean mCameraRunningStateSelected = false;
    private Boolean mRecorderStarted = false;
    private final AtomicBoolean mCameraRunning = new AtomicBoolean(false);
    private HandlerThread mAvailabilityCallbackThread;
    private Handler mAvailabilityCallbackHandler;
    private String mHDMIinCameraID = "";
    private final CameraDisconnectedListener mCameraDisconnectedListenerObject;
    private final Activity mActivity;

    public PresentationBase(Context outerContext, Display display, SettingsUtil data, int index, Activity activity) {
        super(outerContext, display);
        this.mData = data;
        this.mPresentationIndex = index;
        mActivity = activity;
        mCameraDisconnectedListenerObject = this;
        startAvailabilityCallbackThread();
    }

    protected void onCreate(Bundle savedInstanceState) {
        Log.v(TAG, "onCreate enter");
        super.onCreate(savedInstanceState);
        // Set the content view to the custom layout
        // This code is to handle Concurrent Decode functionality
        if (mData.getHDMISource(mPresentationIndex).equals("MP4")) {
            if (mData.getComposeType(mPresentationIndex).equals("OpenGLES")) {
                setContentView(R.layout.opengl_composition);
            } else if (mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode")) {
                setContentView(R.layout.opengles_with_encode);
            } else {
                switch (mData.getDecoderInstanceNumber(mPresentationIndex)) {
                    case 1:
                        setContentView(R.layout.decode_one);
                        break;
                    case 4:
                        setContentView(R.layout.decode_four);
                        break;
                    case 8:
                        setContentView(R.layout.decode_eight);
                        break;
                    case 15:
                        setContentView(R.layout.decode_fifteen);
                        break;
                    case 24:
                        setContentView(R.layout.decode_max);
                        break;
                    default:
                        Log.e(TAG, "Invalid decode configuration");
                }
            }
            Button mSecondaryDisplayBtn = findViewById(R.id.button_decode);
            mSecondaryDisplayBtn.setVisibility(View.INVISIBLE);
            loadFileNames();
        } else if (mData.getHDMISource(mPresentationIndex).equals("Camera") &&
                mData.getIsReprocEnabled(mPresentationIndex)) {
            // This is to handle reproc use case
            setContentView(R.layout.reproc_use_case);
            mReprocButton = findViewById(R.id.reproc_button);
            mReprocButton.setVisibility(View.INVISIBLE);
        }
        else {
            // Load Default layout for Camera
            setContentView(R.layout.secondary_display);
        }
        if (mData.getHDMISource(mPresentationIndex).equals("Camera")) {
            if (mData.getIsReprocEnabled(mPresentationIndex)) {
                mImageView = findViewById(R.id.imageview0);
                mSurfaceViewList.add(findViewById(R.id.surfaceView1));
                mSurfaceViewList.add(findViewById(R.id.surfaceView2));
                mSurfaceViewList.add(findViewById(R.id.surfaceView3));
                for (SurfaceView surface : mSurfaceViewList) {
                    surface.getHolder().addCallback(new SurfaceHolder.Callback() {
                        @Override
                        public void surfaceCreated(SurfaceHolder holder) {
                            holder.setFixedSize(1920,1080);
                            holder.setFormat(ImageFormat.YUV_420_888);
                            createReprocStream();
                        }

                        @Override
                        public void surfaceChanged(SurfaceHolder holder, int format, int width,
                                                   int height) { }

                        @Override
                        public void surfaceDestroyed(SurfaceHolder holder) { }
                    });
                }
            } else {
                SurfaceView mSurfaceView = findViewById(R.id.secondary_surface_view);
                mSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
                    public void surfaceCreated(SurfaceHolder holder) {
                        Log.d(TAG, "surfaceCreated  for Camera");
                        if (mData.getIsHDMIinCameraEnabled(mPresentationIndex)) {
                            mHDMIinSurfaceHolder = holder;
                            CameraManager manager =
                                    (CameraManager) getContext()
                                            .getSystemService(Context.CAMERA_SERVICE);
                            manager.registerAvailabilityCallback(mAvailabilityCallback,
                                    mAvailabilityCallbackHandler);
                        } else {
                            mCameraBase =
                                    new CameraBase(getContext(), mCameraDisconnectedListenerObject);
                            mCameraBase.addPreviewStream(holder);
                            holder.setFixedSize(1920, 1080);
                        }
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
        } else if (mData.getHDMISource(mPresentationIndex).equals("MP4")) {
            if (mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode")) {
                Log.d(TAG, "OpenGLES with Encode is selected");
                mMediaCodecRecorder = new MediaCodecRecorder(getContext(), 1920, 1080, false);
                createMediaCodecDecoderInstances();
            } else {
                createSurfaceView();
            }
        }
        Log.v(TAG, "onCreate exit");
    }


    private final CameraManager.AvailabilityCallback mAvailabilityCallback = new CameraManager.AvailabilityCallback() {
        @Override
        public void onCameraAvailable(@NonNull String cameraId) {
            super.onCameraAvailable(cameraId);
            Log.v(TAG, "onCameraAvailable called with cameraId " + cameraId);
            CameraManager manager =
                    (CameraManager) getContext().getSystemService(Context.CAMERA_SERVICE);
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
                            mActivity.runOnUiThread(() -> {
                                mHDMIinSurfaceHolder.setFixedSize(resolution[0].getWidth(), resolution[0].getHeight());
                            });
                            mCameraBase = new CameraBase(getContext(), mCameraDisconnectedListenerObject);
                            mCameraBase.addPreviewStream(mHDMIinSurfaceHolder);
                            if (mData.getIsHDMIinVideoEnabled(mPresentationIndex)) {
                                mMediaCodecRecorder = new MediaCodecRecorder(getContext(),
                                        resolution[0].getWidth(),
                                        resolution[0].getHeight(),
                                        mData.getIsHDMIinAudioEnabled(mPresentationIndex));
                                mCameraBase.addRecorderStream(
                                        mMediaCodecRecorder.getRecorderSurface());
                            }
                            if (mData.getIsHDMIinAudioEnabled(mPresentationIndex)) {
                                mHDMIinAudioPlayback = new HDMIinAudioPlayback(getContext());
                            }
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        if (mCameraRunningStateSelected && !mCameraRunning.getAndSet(true)) {
                            Log.d(TAG, "onCameraAvailable mCameraRunningStateSelected so will start");
                            mCameraBase.startCamera(mData.getCameraID(mPresentationIndex));
                            if (mMediaCodecRecorder != null) {
                                mMediaCodecRecorder.start(0);
                                mRecorderStarted = true;
                            }
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
                    mActivity.runOnUiThread(() -> {
                        AlertDialog.Builder alert = new AlertDialog.Builder(getContext());
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
                mActivity.runOnUiThread(() -> {
                    AlertDialog.Builder alert = new AlertDialog.Builder(getContext());
                    alert.setTitle("No device is connected");
                    alert.setMessage("To project content connect a device via HDMI.");
                    alert.setPositiveButton("OK", null);
                    alert.show();
                });
            }
            Log.v(TAG, "CameraDisconnectedThread exit");
        }
    }

    public void start() {
        Log.v(TAG, "Starting secondary display Enter");
        mCameraRunningStateSelected = true;
        if (mData.getHDMISource(mPresentationIndex).equals("Camera") &&
                mData.getIsHDMIinCameraEnabled(mPresentationIndex) && !mHDMIinAvailable) {
            if (mData.getHDMISource(mPresentationIndex).equals("MP4") &&
                    mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode") &&
                    mMediaCodecRecorder != null) {
                mMediaCodecRecorder.start(0);
                mRecorderStarted = true;
            }
            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.start();
            }
            AlertDialog.Builder alert = new AlertDialog.Builder(getContext());
            alert.setTitle("No device is connected");
            alert.setMessage("To project content connect a device via HDMI.");
            alert.setPositiveButton("OK", null);
            alert.show();
        } else {
            if (mData.getHDMISource(mPresentationIndex).equals("MP4") &&
                    mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode") &&
                    mMediaCodecRecorder != null) {
                mMediaCodecRecorder.start(0);
                mRecorderStarted = true;
            }
            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.start();
            }
            if (!mCameraRunning.getAndSet(true)) {
                if (mCameraBase != null) { // mCameraRunning won't matter if no camera is there
                    mCameraBase.startCamera(mData.getCameraID(mPresentationIndex));
                }
                if (mData.getHDMISource(mPresentationIndex).equals("Camera") &&
                        mData.getIsHDMIinCameraEnabled(mPresentationIndex)) {
                    if (mHDMIinAudioPlayback != null) {
                        mHDMIinAudioPlayback.start();
                    }
                    if (mMediaCodecRecorder != null) {
                        mMediaCodecRecorder.start(0);
                        mRecorderStarted = true;
                    }
                }
            }
        }
        Log.v(TAG, "Starting secondary display Exit");
    }

    public void stop() {
        Log.v(TAG, "Stopping secondary display Enter");
        mCameraRunningStateSelected = false;
        if (mData.getHDMISource(mPresentationIndex).equals("Camera") &&
                mData.getIsHDMIinCameraEnabled(mPresentationIndex) && !mHDMIinAvailable) {
            if (mData.getHDMISource(mPresentationIndex).equals("MP4") &&
                    mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode") &&
                    mRecorderStarted) {
                mMediaCodecRecorder.stop();
                mRecorderStarted = false;
            }
            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.stop();
            }
        } else {
            if (mData.getHDMISource(mPresentationIndex).equals("MP4") &&
                    mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode") &&
                    mRecorderStarted) {
                mMediaCodecRecorder.stop();
                mRecorderStarted = false;
            }
            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.stop();
            }
            if (mCameraRunning.getAndSet(false)) {
                if (mData.getHDMISource(mPresentationIndex).equals("Camera") &&
                        mData.getIsHDMIinCameraEnabled(mPresentationIndex) && mRecorderStarted) {
                    if (mMediaCodecRecorder != null) {
                        mMediaCodecRecorder.stop();
                        mRecorderStarted = false;
                    }
                    if (mHDMIinAudioPlayback != null) {
                        mHDMIinAudioPlayback.stop();
                    }
                }
                if (mCameraBase != null) { // mCameraRunning won't matter if no camera is there
                    mCameraBase.stopCamera();
                }
            }
        }
        Log.v(TAG, "Stopping secondary display Exit");
    }

    private void createReprocStream() {
        mSurfaceCount++;
        if (mSurfaceCount == mSurfaceViewList.size()) {
            mCameraBase = new CameraBase(getContext(), mCameraDisconnectedListenerObject);
            mCameraBase.enableReproc(mImageView);
            for (SurfaceView surface : mSurfaceViewList) {
                mCameraBase.addReprocStream(surface);
            }
        }
    }

    private void createMediaCodecDecoderInstances() {
        Log.v(TAG, "createMediaCodecDecoderInstances enter");
        int count = mPresentationIndex;
        if (mData.getComposeType(mPresentationIndex).equals("SF")) {
            for (SurfaceView it : mSurfaceViewList) {
                mMediaCodecDecoderList.add(new MediaCodecDecoder(it.getHolder(),
                        it.getHolder().getSurface(),
                        mFileNames.get(count % mFileNames.size())));
                count++;
            }
        } else {
            VideoComposer mVideoComposer;
            if (mData.getComposeType(mPresentationIndex).equals("OpenGLES")) {
                mVideoComposer = new VideoComposer(mSurfaceViewList.get(0).getHolder().getSurface(),
                        1920, 1080, 30f, 0.0f, mData.getDecoderInstanceNumber(mPresentationIndex));
            } else { // This is OPENGL With Encode
                mVideoComposer = new VideoComposer(mMediaCodecRecorder.getRecorderSurface(), 1920,
                        1080, 30.0f, 0.0f, mData.getDecoderInstanceNumber(mPresentationIndex));
            }
            for (int it = 0; it < mData.getDecoderInstanceNumber(mPresentationIndex); it++) {
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

    private void createSurfaceView() {
        Log.v(TAG, "createSurfaceView enter");
        if (mData.getComposeType(mPresentationIndex).equals("OpenGLES")) {
            mSurfaceViewList.add(findViewById(R.id.primary_surface_view));
        } else {
            switch (mData.getDecoderInstanceNumber(mPresentationIndex)) {
                case 1:
                    mSurfaceViewList.add(findViewById(R.id.surfaceView0));
                    break;
                case 4:
                    mSurfaceViewList.add(findViewById(R.id.surfaceView0));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView1));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView2));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView3));
                    break;
                case 8:
                    mSurfaceViewList.add(findViewById(R.id.surfaceView0));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView1));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView2));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView3));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView4));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView5));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView6));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView7));
                    break;
                case 15:
                    mSurfaceViewList.add(findViewById(R.id.surfaceView0));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView1));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView2));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView3));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView4));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView5));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView6));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView7));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView8));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView9));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView10));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView11));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView12));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView13));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView14));
                    break;
                case 24:
                    mSurfaceViewList.add(findViewById(R.id.surfaceView0));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView1));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView2));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView3));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView4));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView5));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView6));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView7));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView8));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView9));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView10));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView11));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView12));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView13));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView14));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView15));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView16));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView17));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView18));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView19));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView20));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView21));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView22));
                    mSurfaceViewList.add(findViewById(R.id.surfaceView23));
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

    private void startAvailabilityCallbackThread() {
        Log.v(TAG, "startAvailabilityCallbackThread enter");
        mAvailabilityCallbackThread = new HandlerThread("AvailabilityCallbackThread");
        mAvailabilityCallbackThread.start();
        mAvailabilityCallbackHandler = new Handler(mAvailabilityCallbackThread.getLooper());
        Log.v(TAG, "startAvailabilityCallbackThread exit");
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
    public void onDetachedFromWindow() {
        Log.v(TAG, "onDetachedFromWindow enter");
        super.onDetachedFromWindow();
        CameraManager manager =
                (CameraManager) getContext().getSystemService(Context.CAMERA_SERVICE);
        manager.unregisterAvailabilityCallback(mAvailabilityCallback);
        stopAvailabilityCallbackThread();
        Log.v(TAG, "onDetachedFromWindow exit");
    }
}


