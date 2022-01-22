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

import android.graphics.PixelFormat;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

import java.io.IOException;
import java.nio.ByteBuffer;


public final class MediaCodecDecoder {

    private static final String VIDEO = "video/";
    private static final String TAG = "MediaCodecDecoder";
    private final Surface mSurface;
    private final String mClipPath;
    private SurfaceHolder mSurfaceHolder = null;
    private MediaCodec mVideoDecoder;
    private MediaExtractor mExtractor;
    private Boolean mVideoDecoderRunning = false;
    private Thread mVideoDecoderThread;
    private boolean mIsFirstFrameReceived;
    private long mFirstSampleTimestampUs;
    private long mFirstSampleReceivedTimeUs;

    public MediaCodecDecoder(SurfaceHolder surfaceHolder, Surface surface, String clipPath) {
        this.mClipPath = clipPath;
        this.mSurface = surface;
        this.mSurfaceHolder = surfaceHolder;
    }

    public void start() {
        Log.v(TAG, "start enter");
        mIsFirstFrameReceived = false;
        mExtractor = new MediaExtractor();
        try {
            mExtractor.setDataSource(mClipPath);
        } catch (IOException e) {
            e.printStackTrace();
        }
        for (int index = 0; index <= mExtractor.getTrackCount(); index++) {
            MediaFormat format = mExtractor.getTrackFormat(index);
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (mime != null && mime.startsWith(VIDEO)) {
                mExtractor.selectTrack(index);
                try {
                    mVideoDecoder = MediaCodec.createDecoderByType(mime);
                } catch (IOException e) {
                    e.printStackTrace();
                }
                try {
                    Log.i(TAG, "format : " + format);
                    // Enable Low Latency Mode
                    format.setInteger("vendor.qti-ext-dec-low-latency.enable", 1);
                    // Real Time Priority
                    format.setInteger(MediaFormat.KEY_PRIORITY, 0);
                    mVideoDecoder.configure(format, mSurface, null, 0 /* Decode */);
                } catch (Exception e) {
                    e.printStackTrace();
                }
                break;
            }
        }
        mVideoDecoder.start();
        mVideoDecoderThread = new videoDecoderHandler(false);
        mVideoDecoderThread.start();
        Log.v(TAG, "start exit");
    }


    public void stop() {
        Log.v(TAG, "stop enter");
        mVideoDecoderRunning = false;
        try {
            mVideoDecoderThread.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        // Clear the surface
        if (mSurfaceHolder != null) {
            mSurfaceHolder.setFormat(PixelFormat.TRANSPARENT);
            mSurfaceHolder.setFormat(PixelFormat.OPAQUE);
        }
        Log.v(TAG, "stop exit");
    }


    class videoDecoderHandler extends Thread {
        boolean endOfStream;

        public videoDecoderHandler(boolean endOfStream) {
            this.endOfStream = endOfStream;
        }

        //method where the thread execution will start
        public void run() {
            Log.v(TAG, "videoDecoderHandler enter");
            //logic to execute in a thread
            mVideoDecoderRunning = true;
            MediaCodec.BufferInfo newBufferInfo = new MediaCodec.BufferInfo();
            ByteBuffer[] inputBuffers = mVideoDecoder.getInputBuffers();
            ByteBuffer[] outputBuffers = mVideoDecoder.getOutputBuffers();

            while (mVideoDecoderRunning) {
                int index = mVideoDecoder.dequeueInputBuffer(1000);
                if (index >= 0) {
                    // fill inputBuffers[inputBufferIndex] with valid data
                    ByteBuffer inputBuffer = inputBuffers[index];
                    int sampleSize = mExtractor.readSampleData(inputBuffer, 0);

                    if (sampleSize >= 0) {
                        mVideoDecoder.queueInputBuffer(index, 0, sampleSize,
                                mExtractor.getSampleTime(), 0);
                    } else {
                        Log.d(TAG, "InputBuffer BUFFER_FLAG_END_OF_STREAM");
                        mVideoDecoder.queueInputBuffer(index, 0, 0,
                                0, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                    }
                    mExtractor.advance();
                }
                int outIndex = mVideoDecoder.dequeueOutputBuffer(newBufferInfo, 1000);

                switch (outIndex) {
                    case MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED:
                        Log.d(TAG, "INFO_OUTPUT_BUFFERS_CHANGED");
                        outputBuffers = mVideoDecoder.getOutputBuffers();
                        break;
                    case MediaCodec.INFO_OUTPUT_FORMAT_CHANGED:
                        Log.d(TAG, "INFO_OUTPUT_FORMAT_CHANGED format : " +
                                mVideoDecoder.getOutputFormat());
                        break;
                    case MediaCodec.INFO_TRY_AGAIN_LATER:
                        Log.d(TAG, "INFO_TRY_AGAIN_LATER");
                        break;
                    default:
                        boolean render = newBufferInfo.size != 0;
                        if (!mIsFirstFrameReceived) {
                            mIsFirstFrameReceived = true;
                            mFirstSampleReceivedTimeUs = System.nanoTime() / 1000;
                            mFirstSampleTimestampUs = newBufferInfo.presentationTimeUs;
                        }
                        long receivedTimeDelta = System.nanoTime() / 1000 - mFirstSampleReceivedTimeUs;
                        long presentationTimeDelta = newBufferInfo.presentationTimeUs - mFirstSampleTimestampUs;

                        try {
                            if (presentationTimeDelta > receivedTimeDelta) {
                                Thread.sleep((presentationTimeDelta - receivedTimeDelta) / 1000);
                            }
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                        mVideoDecoder.releaseOutputBuffer(outIndex, render);
                }

                // All decoded frames have been rendered, we can stop playing now
                if ((newBufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                    Log.d(TAG, "OutputBuffer BUFFER_FLAG_END_OF_STREAM");
                    break;
                }

                if (endOfStream) {
                    break;
                }
            }
            mVideoDecoder.stop();
            mVideoDecoder.release();
            mExtractor.release();
            Log.v(TAG, "videoDecoderHandler exit");
        }
    }
}
