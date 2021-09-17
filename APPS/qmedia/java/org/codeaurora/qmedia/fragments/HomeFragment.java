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

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.Toast;

import androidx.fragment.app.Fragment;

import org.codeaurora.qmedia.CameraBase;
import org.codeaurora.qmedia.MediaCodecDecoder;
import org.codeaurora.qmedia.MediaCodecRecorder;
import org.codeaurora.qmedia.PresentationBase;
import org.codeaurora.qmedia.R;
import org.codeaurora.qmedia.SettingsUtil;
import org.codeaurora.qmedia.opengles.VideoComposer;

import java.io.File;
import java.util.ArrayList;

public class HomeFragment extends Fragment {

    private static final String TAG = "HomeFragment";
    private static final String FILES_PATH = "/storage/emulated/0/DCIM/Test";

    private final ArrayList<SurfaceView> mSurfaceViewList = new ArrayList<>();
    private final ArrayList<MediaCodecDecoder> mMediaCodecDecoderList = new ArrayList<>();
    private final ArrayList<PresentationBase> mPresentationBaseList = new ArrayList<>();
    private SettingsUtil mSettingData;
    private ArrayList<String> mFileNames;
    private Boolean mPrimaryDisplayStarted = false;
    private int mSurfaceCount = 0;

    private MediaCodecDecoder mMediaCodecDecoder = null;
    private VideoComposer mVideoComposer = null;
    private DisplayManager mDisplayManager;
    private SurfaceView mPrimaryDisplaySurfaceView;
    private Button mPrimaryDisplayButton;
    private CameraBase mCameraBase = null;
    private MediaCodecRecorder mMediaCodecRecorder = null;

    public HomeFragment() {
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        mSettingData = new SettingsUtil(getActivity().getApplicationContext());
        mSettingData.printSettingsValues();

        // This code is to handle Concurrent Decode functionality
        if (mSettingData.getDecodeStatus()) {
            if (mSettingData.getComposeType().equals("OpenGLES")) {
                return inflater.inflate(R.layout.opengl_composition, container, false);
            } else if (mSettingData.getComposeType().equals("OpenGLESWithEncode")) {
                return inflater.inflate(R.layout.opengles_with_encode, container, false);
            } else {
                switch (mSettingData.getDecodeInstance()) {
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
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        loadFileNames();
        if (mSettingData.getConcurrentHDMIStatus()) {
            handleConcurrentHDMI(view);
        }

        if (mSettingData.getDecodeStatus()) {
            handleConcurrentDecode(view);
        }

        if (mSettingData.getHDMIInStatus()) {
            handleHDMIIn(view);
        }

    }

    @Override
    public void onResume() {
        super.onResume();
        if (mSettingData.getConcurrentHDMIStatus()) {
            int filePosition = 1;
            Display[] displays = mDisplayManager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);
            Log.d(TAG, "Number of display" + displays.length);
            ArrayList<String> source = mSettingData.getConcurrentHDMISource();

            for (int it = 0; it < displays.length ; it++) {
                if (source.get(it+1).equals("Camera")) {
                    mPresentationBaseList
                            .add(new PresentationBase(getContext(), displays[it],
                                    String.valueOf(it+1)));
                } else if (source.get(it+1).equals("MP4")) {
                    mPresentationBaseList.add(new PresentationBase(getContext(),
                            displays[it],
                            (mFileNames.size() > 2) ? mFileNames.get(filePosition++) :
                                    mFileNames.get(filePosition)));
                }
            }

            for (PresentationBase it : mPresentationBaseList) {
                try {
                    it.show();
                } catch (WindowManager.InvalidDisplayException exception) {
                    mPresentationBaseList.remove(it);
                }
            }
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mPrimaryDisplayStarted) {
            for (PresentationBase it : mPresentationBaseList) {
                it.stop();
            }
            if (mMediaCodecDecoder != null) {
                mMediaCodecDecoder.stop();
            }

            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.stop();
            }

            if (mMediaCodecRecorder != null) {
                mMediaCodecRecorder.stop();
            }
            mPrimaryDisplayStarted = false;
            mPrimaryDisplayButton.setText("Start");

            mMediaCodecDecoder = null;
            for (PresentationBase it : mPresentationBaseList) {
                it.dismiss();
            }

            // This will handle primary screen camera op
            if (mCameraBase != null) {
                mCameraBase.closeCamera();
                mPrimaryDisplayButton.setText("Start");
                mCameraBase = null;
            }
        }

    }

    private void processConcurrentHDMI() {
        mPrimaryDisplayStarted = !mPrimaryDisplayStarted;
        for (PresentationBase it : mPresentationBaseList) {
            if (mPrimaryDisplayStarted)
                it.start();
            else
                it.stop();
        }
        // This will handle primary screen decode op.
        if (mMediaCodecDecoder != null) {
            if (mPrimaryDisplayStarted) {
                mMediaCodecDecoder.start();
                mPrimaryDisplayButton.setText("Stop");
            } else {
                mMediaCodecDecoder.stop();
                mPrimaryDisplayButton.setText("Start");
            }
        }
        // This will handle primary screen camera op.
        if (mCameraBase != null) {
            if (mPrimaryDisplayStarted) {
                mCameraBase.startCamera("0");
                mPrimaryDisplayButton.setText("Stop");
            } else {
                mCameraBase.closeCamera();
                mPrimaryDisplayButton.setText("Start");
            }
        }
    }

    private void processConcurrentDecode() {
        mPrimaryDisplayStarted = !mPrimaryDisplayStarted;
        if (mPrimaryDisplayStarted) {

            if (mSettingData.getComposeType().equals("OpenGLESWithEncode")) {
                mMediaCodecRecorder.start(0);
            }
            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.start();
            }
            mPrimaryDisplayButton.setText("Stop");
        } else {
            if (mSettingData.getComposeType().equals("OpenGLESWithEncode")) {
                mMediaCodecRecorder.stop();
            }
            for (MediaCodecDecoder it : mMediaCodecDecoderList) {
                it.stop();
            }

            mPrimaryDisplayButton.setText("Start");
        }
    }

    private void processHDMIIn() {
        mPrimaryDisplayStarted = !mPrimaryDisplayStarted;
        // This will handle primary screen camera op.
        if (mCameraBase != null) {
            if (mPrimaryDisplayStarted) {
                mCameraBase.startCamera("3");
                mPrimaryDisplayButton.setText("Stop");
            } else {
                mCameraBase.closeCamera();
                mPrimaryDisplayButton.setText("Start");
            }
        }
    }


    private void handleHDMIIn(View view) {
        mPrimaryDisplayButton = view.findViewById(R.id.primary_display_button);
        mPrimaryDisplayButton.setOnClickListener((View v) -> {
            processHDMIIn();
        });
        mPrimaryDisplaySurfaceView = view.findViewById(R.id.primary_surface_view);
        mPrimaryDisplaySurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                mPrimaryDisplayButton.setEnabled(true);
                holder.setFixedSize(1920, 1080);
                mCameraBase = new CameraBase(getContext());
                mCameraBase.addPreviewStream(holder);
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {

            }
        });
    }

    private void handleConcurrentHDMI(View view) {
        mPrimaryDisplayButton = view.findViewById(R.id.primary_display_button);
        mPrimaryDisplayButton.setOnClickListener((View v) -> {
            processConcurrentHDMI();
        });
        mDisplayManager = (DisplayManager) getActivity().getSystemService(Context.DISPLAY_SERVICE);
        mPrimaryDisplaySurfaceView = view.findViewById(R.id.primary_surface_view);
        mPrimaryDisplaySurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                mPrimaryDisplayButton.setEnabled(true);
                ArrayList<String> source = mSettingData.getConcurrentHDMISource();
                if (source.get(0).equals("Camera")) {
                    holder.setFixedSize(1920, 1080);
                    mCameraBase = new CameraBase(getContext());
                    mCameraBase.addPreviewStream(holder);
                } else if (source.get(0).equals("MP4")) {
                    mMediaCodecDecoder =
                            new MediaCodecDecoder(mPrimaryDisplaySurfaceView.getHolder(),
                                    mPrimaryDisplaySurfaceView.getHolder().getSurface(),
                                    mFileNames.get(0));
                }
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {

            }
        });
    }

    private void handleConcurrentDecode(View view) {
        mPrimaryDisplayButton = view.findViewById(R.id.button_decode);
        mPrimaryDisplayButton.setOnClickListener((View v) -> {
            processConcurrentDecode();
        });
        if (mSettingData.getComposeType().equals("OpenGLESWithEncode")) {
            Log.i(TAG, "OpenGLES with Encode is selected");
            mMediaCodecRecorder = new MediaCodecRecorder(getContext(), 1920, 1080, false);
            createMediaCodecDecoderInstances();
        } else {
            createSurfaceView(view);
        }
    }

    private void createSurfaceView(View view) {
        if (mSettingData.getComposeType().equals("OpenGLES")) {
            mSurfaceViewList.add(view.findViewById(R.id.primary_surface_view));
        } else {
            switch (mSettingData.getDecodeInstance()) {
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
    }

    private void updateSurfaceCreatedCount() {
        mSurfaceCount++;
        if (mSurfaceCount == mSurfaceViewList.size()) {
            createMediaCodecDecoderInstances();
        }
    }

    private void createMediaCodecDecoderInstances() {
        int count = 0;
        if (mSettingData.getComposeType().equals("SF")) {
            for (SurfaceView it : mSurfaceViewList) {
                mMediaCodecDecoderList.add(new MediaCodecDecoder(it.getHolder(),
                        it.getHolder().getSurface(),
                        mFileNames.get(count % mFileNames.size())));
                count++;
            }
        } else {
            if (mSettingData.getComposeType().equals("OpenGLES")) {
                mVideoComposer = new VideoComposer(mSurfaceViewList.get(0).getHolder().getSurface(),
                        1280, 720, 30f, 0.0f, mSettingData.getDecodeInstance());
            } else { // This is OPENGL With Encode
                mVideoComposer = new VideoComposer(mMediaCodecRecorder.getRecorderSurface(), 1920,
                        1080, 30.0f, 0.0f, mSettingData.getDecodeInstance());
            }
            for (int it = 0; it < mSettingData.getDecodeInstance(); it++) {
                mMediaCodecDecoderList.add(new MediaCodecDecoder(null,
                        mVideoComposer.getInputSurface(it),
                        mFileNames.get(count % mFileNames.size())));
                count++;
            }
        }
    }

    private void loadFileNames() {
        mFileNames = new ArrayList<>();
        try {
            File[] files = new File(FILES_PATH).listFiles();
            for (File file : files) {
                if (file.isFile()) {
                    Log.d(TAG, "File Names: " + file.getAbsolutePath());
                    mFileNames.add(file.getAbsolutePath());
                }
            }
        } catch (NullPointerException e) {
            Toast.makeText(getContext(), "Please provide the correct path for video files",
                    Toast.LENGTH_SHORT).show();
            e.printStackTrace();
        }
    }
}