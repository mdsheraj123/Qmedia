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


public class PresentationBase extends Presentation {

    private static final String TAG = "PresentationBase";

    SurfaceView mSurfaceView;
    MediaCodecDecoder mMediaCodecDecoder = null;
    CameraBase mCameraBase = null;
    String mData;

    public PresentationBase(Context outerContext, Display display, String data) {
        super(outerContext, display);
        this.mData = data;
    }

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Set the content view to the custom layout
        setContentView(R.layout.secondary_display);

        mSurfaceView = findViewById(R.id.secondary_surface_view);
        mSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            public void surfaceCreated(SurfaceHolder holder) {
                if (mData.equals("0") || mData.equals("1") || mData.equals("2")) {
                    // This is camera use case
                    mCameraBase = new CameraBase(getContext());
                    mCameraBase.addPreviewStream(holder);
                } else {
                    mMediaCodecDecoder = new MediaCodecDecoder(holder, holder.getSurface(), mData);
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

    public void start() {
        Log.d(TAG, "Starting secondary display");
        if (mMediaCodecDecoder != null)
            mMediaCodecDecoder.start();
        if (mCameraBase != null)
            mCameraBase.startCamera(mData);
    }

    public void stop() {
        Log.d(TAG, "Stopping secondary display");
        if (mMediaCodecDecoder != null)
            mMediaCodecDecoder.stop();
        if (mCameraBase != null)
            mCameraBase.closeCamera();
    }
}


