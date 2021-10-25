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

import android.app.Presentation;
import android.content.Context;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import org.codeaurora.qmedia.opengles.VideoComposer;

import java.io.File;
import java.util.ArrayList;


public class PresentationBase extends Presentation {

    private static final String TAG = "PresentationBase";
    private static final String FILES_PATH = "/storage/emulated/0/DCIM/Test";
    private final ArrayList<MediaCodecDecoder> mMediaCodecDecoderList = new ArrayList<>();
    private final ArrayList<SurfaceView> mSurfaceViewList = new ArrayList<>();
    private SurfaceView mSurfaceView;
    private CameraBase mCameraBase = null;
    private final SettingsUtil mData;
    private final int mPresentationIndex;
    private Button mSecondaryDisplayBtn;
    private MediaCodecRecorder mMediaCodecRecorder = null;
    private VideoComposer mVideoComposer = null;
    private ArrayList<String> mFileNames;
    private int mSurfaceCount = 0;

    private HDMIinAudioPlayback mHDMIinAudioPlayback = null;

    public PresentationBase(Context outerContext, Display display, SettingsUtil data, int index) {
        super(outerContext, display);
        this.mData = data;
        this.mPresentationIndex = index;
    }

    protected void onCreate(Bundle savedInstanceState) {
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
            mSecondaryDisplayBtn = findViewById(R.id.button_decode);
            mSecondaryDisplayBtn.setVisibility(View.INVISIBLE);
            loadFileNames();
        } else {
            // Load Default layout for Camera
            setContentView(R.layout.secondary_display);
        }
        if (mData.getHDMISource(mPresentationIndex).equals("Camera")) {
            mSurfaceView = findViewById(R.id.secondary_surface_view);
            mSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
                public void surfaceCreated(SurfaceHolder holder) {
                    holder.setFixedSize(1920, 1080);
                    mCameraBase = new CameraBase(getContext());
                    mCameraBase.addPreviewStream(holder);
                    if (mData.getCameraID(mPresentationIndex).equals("3")) {
                        mMediaCodecRecorder = new MediaCodecRecorder(getContext(),
                                3840, 2160, true);
                        mCameraBase.addRecorderStream(mMediaCodecRecorder.getRecorderSurface());
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
            if (mData.getCameraID(mPresentationIndex).equals("3")) {
                mHDMIinAudioPlayback = new HDMIinAudioPlayback(getContext().getApplicationContext());
            }
        } else if (mData.getHDMISource(mPresentationIndex).equals("MP4")) {
            if (mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode")) {
                Log.i(TAG, "OpenGLES with Encode is selected");
                mMediaCodecRecorder = new MediaCodecRecorder(getContext(), 1920, 1080, false);
                createMediaCodecDecoderInstances();
            } else {
                createSurfaceView();
            }
        }
    }

    public void start() {
        Log.v(TAG, "Starting secondary display Enter");
        if (mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode") &&
                mMediaCodecRecorder != null) {
            mMediaCodecRecorder.start(0);
        }
        for (MediaCodecDecoder it : mMediaCodecDecoderList) {
            it.start();
        }
        if (mCameraBase != null) {
            mCameraBase.startCamera(mData.getCameraID(mPresentationIndex));
        }
        if (mData.getCameraID(mPresentationIndex).equals("3")) {
            mHDMIinAudioPlayback.start();
            mMediaCodecRecorder.start(0);
        }
        Log.v(TAG, "Starting secondary display Exit");
    }

    public void stop() {
        Log.v(TAG, "Stopping secondary display Enter");
        if (mData.getComposeType(mPresentationIndex).equals("OpenGLESWithEncode") &&
                mMediaCodecRecorder != null) {
            mMediaCodecRecorder.stop();
        }
        for (MediaCodecDecoder it : mMediaCodecDecoderList) {
            it.stop();
        }
        if (mData.getCameraID(mPresentationIndex).equals("3")) {
            mMediaCodecRecorder.stop();
            mHDMIinAudioPlayback.stop();
        }
        if (mCameraBase != null) {
            mCameraBase.closeCamera();
        }
        Log.v(TAG, "Stopping secondary display Exit");
    }

    private void createMediaCodecDecoderInstances() {
        int count = mPresentationIndex;
        if (mData.getComposeType(mPresentationIndex).equals("SF")) {
            for (SurfaceView it : mSurfaceViewList) {
                mMediaCodecDecoderList.add(new MediaCodecDecoder(it.getHolder(),
                        it.getHolder().getSurface(),
                        mFileNames.get(count % mFileNames.size())));
                count++;
            }
        } else {
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

    private void createSurfaceView() {
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
    }

    private void updateSurfaceCreatedCount() {
        mSurfaceCount++;
        if (mSurfaceCount == mSurfaceViewList.size()) {
            createMediaCodecDecoderInstances();
        }
    }
}


