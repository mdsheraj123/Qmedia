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

    public MediaCodecDecoder(SurfaceHolder surfaceHolder, Surface surface, String clipPath) {
        this.mClipPath = clipPath;
        this.mSurface = surface;
        this.mSurfaceHolder = surfaceHolder;
    }

    public void start() {
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
                    format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 65536);
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
    }


    public void stop() {
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
    }


    class videoDecoderHandler extends Thread {
        boolean endOfStream;

        public videoDecoderHandler(boolean endOfStream) {
            this.endOfStream = endOfStream;
        }

        //method where the thread execution will start
        public void run() {
            //logic to execute in a thread
            mVideoDecoderRunning = true;
            MediaCodec.BufferInfo newBufferInfo = new MediaCodec.BufferInfo();
            ByteBuffer[] inputBuffers = mVideoDecoder.getInputBuffers();
            ByteBuffer[] outputBuffers = mVideoDecoder.getOutputBuffers();
            long startMs = System.currentTimeMillis();

            while (mVideoDecoderRunning) {
                int index = mVideoDecoder.dequeueInputBuffer(1000);
                if (index >= 0) {
                    // fill inputBuffers[inputBufferIndex] with valid data
                    ByteBuffer inputBuffer = inputBuffers[index];
                    int sampleSize = mExtractor.readSampleData(inputBuffer, 0);

                    if (mExtractor.advance() && sampleSize > 0) {
                        mVideoDecoder
                                .queueInputBuffer(index, 0, sampleSize, mExtractor.getSampleTime(),
                                        0);
                    } else {
                        Log.d(TAG, "InputBuffer BUFFER_FLAG_END_OF_STREAM");
                        mVideoDecoder.queueInputBuffer(
                                index,
                                0,
                                0,
                                0,
                                MediaCodec.BUFFER_FLAG_END_OF_STREAM
                        );
                    }
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
                        // We use a very simple clock to keep the video FPS, or the video
                        // playback will be too fast
                        while (newBufferInfo.presentationTimeUs / 1000 >
                                System.currentTimeMillis() - startMs) {
                            try {
                                sleep(10);
                            } catch (InterruptedException e) {
                                e.printStackTrace();
                            }
                        }
                        boolean render = newBufferInfo.size != 0;
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
        }
    }

}
