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

import android.content.ContentValues;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.AudioTimestamp;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.media.MediaRecorder;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.MediaStore.Video.Media;
import android.util.Log;
import android.view.Surface;

import java.io.FileDescriptor;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.Semaphore;

public class MediaCodecRecorder {
    private static final long TIMEOUT_USEC = 10000L;
    private static final String TAG = "MediaCodecRecorder";
    private static final int AUDIO_SAMPLE_RATE = 44100;
    private static final int AUDIO_SAMPLES_PER_FRAME = 1024;
    private static final int AUDIO_BITRATE = 128000;
    private static final String AUDIO_MIME_TYPE = "audio/mp4a-latm";
    private final Context mContext;
    private final Semaphore mMuxerLock = new Semaphore(1);
    volatile long mPresentationTimeUs = -1L;
    private final Surface mSurface = MediaCodec.createPersistentInputSurface();
    private MediaCodec mVideoEncoder;
    private MediaMuxer mMuxer;
    private boolean mVideoEncoderRunning;
    private boolean mMuxerCreated;
    private boolean mMuxerStarted;
    private int mVideoTrackIndex;
    private final MediaFormat mVideoFormat;
    private int mMuxerTrackCount;
    private Thread mVideoEncoderThread;
    private boolean mIsFirstTime;
    private final boolean mIsAudioEnabled;
    private boolean mAudioEncoderRunning = false;
    private boolean mAudioRecorderRunning = false;
    private int mAudioTrackIndex = -1;
    private AudioRecord mAudioRecord;
    private MediaFormat mAudioFormat;
    private MediaCodec mAudioEncoder;
    private Thread mAudioEncoderThread;
    private Thread mAudioRecorderThread;

    public MediaCodecRecorder(Context context, int width, int height, boolean audio_enabled) {
        this.mContext = context;
        this.mIsAudioEnabled = audio_enabled;
        this.mVideoFormat = MediaFormat.createVideoFormat("video/avc", width, height);
        this.mVideoFormat.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
        this.mVideoFormat.setInteger(MediaFormat.KEY_BIT_RATE, 10 * 1000000);
        this.mVideoFormat.setInteger(MediaFormat.KEY_FRAME_RATE, 30);

        this.mVideoFormat.setInteger("vendor.qti-ext-enc-bitrate-mode.value", 2);

        // Real Time Priority
        this.mVideoFormat.setInteger(MediaFormat.KEY_PRIORITY, 0);

        this.mVideoFormat.setInteger("vendor.qti-ext-enc-qp-range.qp-i-min", 10);
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-qp-range.qp-i-max", 51);
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-qp-range.qp-b-min", 10);
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-qp-range.qp-b-max", 51);
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-qp-range.qp-p-min", 10);
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-qp-range.qp-p-max", 51);

        this.mVideoFormat.setInteger("vendor.qti-ext-enc-initial-qp.qp-i-enable", 1);
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-initial-qp.qp-i", 10);

        this.mVideoFormat.setInteger("vendor.qti-ext-enc-initial-qp.qp-b-enable", 1);
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-initial-qp.qp-b", 10);

        this.mVideoFormat.setInteger("vendor.qti-ext-enc-initial-qp.qp-p-enable", 1);
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-initial-qp.qp-p", 10);

        this.mVideoFormat.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
        int pFrameCount = 29;
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-intra-period.n-pframes", pFrameCount);
        // Always set B frames to 0.
        this.mVideoFormat.setInteger("vendor.qti-ext-enc-intra-period.n-bframes", 0);
        this.mVideoFormat.setInteger(MediaFormat.KEY_MAX_B_FRAMES, 0);

        mVideoEncoder = createVideoEncoder();
        mVideoEncoder.start();

        mVideoEncoderThread = new VideoEncoderThread(true);
        mVideoEncoderThread.start();
        try {
            mVideoEncoderThread.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        if (mIsAudioEnabled) {
            int minBufferSize = AudioRecord.getMinBufferSize(AUDIO_SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT);

            int bufferSize = AUDIO_SAMPLES_PER_FRAME * 10;
            if (bufferSize < minBufferSize) bufferSize =
                    (minBufferSize / AUDIO_SAMPLES_PER_FRAME + 1) * AUDIO_SAMPLES_PER_FRAME * 2;
            mAudioRecord = new AudioRecord(MediaRecorder.AudioSource.CAMCORDER, AUDIO_SAMPLE_RATE
                    , AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, bufferSize);
            mAudioFormat = new MediaFormat();
            mAudioFormat.setString(MediaFormat.KEY_MIME, AUDIO_MIME_TYPE);
            mAudioFormat.setInteger(MediaFormat.KEY_AAC_PROFILE,
                    MediaCodecInfo.CodecProfileLevel.AACObjectLC);
            mAudioFormat.setInteger(MediaFormat.KEY_SAMPLE_RATE, AUDIO_SAMPLE_RATE);
            mAudioFormat.setInteger(MediaFormat.KEY_CHANNEL_COUNT, 1);
            mAudioFormat.setInteger(MediaFormat.KEY_BIT_RATE, AUDIO_BITRATE);
        }
    }

    private MediaCodec createVideoEncoder() {
        MediaCodec obj = null;
        try {
            obj = MediaCodec.createEncoderByType("video/avc");
            obj.configure(mVideoFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            obj.setInputSurface(mSurface);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return obj;
    }

    private MediaCodec createAudioIOEncoder() {
        MediaCodec obj = null;
        try {
            obj = MediaCodec.createEncoderByType(AUDIO_MIME_TYPE);
            obj.configure(mAudioFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
        } catch (IOException e) {
            e.printStackTrace();
        }
        return obj;
    }

    private MediaMuxer createMuxer() throws IOException {
        mMuxerCreated = true;
        return new MediaMuxer(createVideoFile(), MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
    }

    private void releaseVideoEncoder() {
        this.mVideoEncoderRunning = false;
        this.mVideoEncoder.stop();
        this.mVideoEncoder.release();
    }

    private void releaseAudioEncoder() {
        mAudioEncoderRunning = false;
        mAudioEncoder.stop();
        mAudioEncoder.release();
    }

    private void releaseMuxer() {
        Log.i(TAG, "releaseMuxer");
        if (this.mMuxerStarted) {
            this.mMuxer.stop();
        }

        if (this.mMuxerCreated) {
            this.mMuxer.release();
        }

        this.mMuxerCreated = false;
        this.mMuxerStarted = false;
        this.mMuxerTrackCount = 0;
    }

    public void start(int orientation) {
        Log.i(TAG, "start enter");
        try {
            mMuxer = createMuxer();
        } catch (IOException e) {
            e.printStackTrace();
        }
        mMuxer.setOrientationHint(orientation);
        mVideoEncoder = createVideoEncoder();
        mVideoEncoder.start();

        mVideoEncoderThread = new VideoEncoderThread(false);
        mVideoEncoderThread.start();

        if (mIsAudioEnabled) {
            mAudioEncoder = createAudioIOEncoder();
            mAudioEncoder.start();

            mAudioEncoderThread = new AudioEncoderThread(false);
            mAudioEncoderThread.start();

            mAudioRecord.startRecording();
            mAudioRecorderThread = new AudioRecorderThread(false);
            mAudioRecorderThread.start();
        }

        Log.i(TAG, "start exit");
    }

    public void stop() {
        Log.i(TAG, "stop enter");
        mVideoEncoderRunning = false;
        try {
            mVideoEncoderThread.join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }

        if (mIsAudioEnabled) {
            mAudioRecorderRunning = false;
            try {
                mAudioEncoderThread.join();
                mAudioRecorderThread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        releaseMuxer();
        Log.i(TAG, "stop exit");
    }

    public final Surface getRecorderSurface() {
        return this.mSurface;
    }

    private FileDescriptor createVideoFile() {
        long dateTaken = System.currentTimeMillis();
        String filename = "VID_" +
                (new SimpleDateFormat("yyyy_MM_dd_HH_mm_ss_SSS", Locale.US)).format(new Date()) +
                ".mp4";

        Log.d(TAG, "Recorder output file: " + filename);
        ContentValues values = new ContentValues();
        values.put(Media.TITLE, filename);
        values.put(Media.DISPLAY_NAME, filename);
        values.put(Media.DATE_TAKEN, dateTaken);
        values.put(Media.MIME_TYPE, "video/mp4");
        values.put(Media.RELATIVE_PATH, Environment.DIRECTORY_DCIM + "/Camera");
        Uri uri = this.mContext.getContentResolver().insert(Media.EXTERNAL_CONTENT_URI, values);

        ParcelFileDescriptor file = null;
        try {
            file = mContext.getContentResolver().openFileDescriptor(uri, "w");
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        }

        assert file != null;
        return file.getFileDescriptor();

    }

    class VideoEncoderThread extends Thread {
        boolean endOfStream;

        public VideoEncoderThread(boolean endOfStream) {
            this.endOfStream = endOfStream;
        }

        //method where the thread execution will start
        public void run() {
            mVideoEncoderRunning = true;
            ByteBuffer[] encoderOutputBuffers = mVideoEncoder.getOutputBuffers();
            while (mVideoEncoderRunning) {
                MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
                int encoderStatus = mVideoEncoder.dequeueOutputBuffer(bufferInfo, TIMEOUT_USEC);
                if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    try {
                        mMuxerLock.acquire();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    if (mMuxerCreated) {
                        mMuxerTrackCount++;
                        mVideoTrackIndex = mMuxer.addTrack(mVideoEncoder.getOutputFormat());
                        if (mIsAudioEnabled) {
                            if (mMuxerTrackCount == 2 && !mMuxerStarted) {
                                mMuxer.start();
                                mMuxerStarted = true;
                            }
                        } else {
                            if (mMuxerTrackCount == 1 && !mMuxerStarted) {
                                mMuxer.start();
                                mMuxerStarted = true;
                            }
                        }
                    }
                    mMuxerLock.release();
                } else if (encoderStatus < 0) {

                } else {
                    if (endOfStream) {
                        mVideoEncoder.releaseOutputBuffer(encoderStatus, false);
                        break;
                    }

                    ByteBuffer encodedData = encoderOutputBuffers[encoderStatus];
                    if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                        bufferInfo.size = 0;
                    }

                    if (bufferInfo.size != 0 && mMuxerStarted) {
                        if (mIsFirstTime) {
                            mIsFirstTime = false;
                            Log.e(TAG, "First Video Frame received.");
                        }
                        encodedData.position(bufferInfo.offset);
                        encodedData.limit(bufferInfo.offset + bufferInfo.size);
                        if(mIsAudioEnabled) {
                            while (mPresentationTimeUs == -1L) {
                            }
                            bufferInfo.presentationTimeUs = mPresentationTimeUs;
                        }
                        mMuxer.writeSampleData(mVideoTrackIndex, encodedData, bufferInfo);
                    }
                    mVideoEncoder.releaseOutputBuffer(encoderStatus, false);

                    if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        break;
                    }
                }
                if (endOfStream) {
                    break;
                }
            }
            releaseVideoEncoder();
        }
    }

    class AudioEncoderThread extends Thread {
        boolean endOfStream;

        public AudioEncoderThread(boolean endOfStream) {
            this.endOfStream = endOfStream;
        }

        //method where the thread execution will start
        public void run() {
            mAudioEncoderRunning = true;
            ByteBuffer[] encoderOutputBuffers = mAudioEncoder.getOutputBuffers();
            long oldTimeStampUs = 0;
            while (mAudioEncoderRunning) {
                MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
                int encoderStatus = mAudioEncoder.dequeueOutputBuffer(bufferInfo, TIMEOUT_USEC);
                if (encoderStatus == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                    try {
                        mMuxerLock.acquire();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    if (mMuxerCreated) {
                        mMuxerTrackCount++;
                        mAudioTrackIndex = mMuxer.addTrack(mAudioEncoder.getOutputFormat());
                        if (mMuxerTrackCount == 2 && !mMuxerStarted) {
                            mMuxer.start();
                            mMuxerStarted = true;
                        }
                    }
                    mMuxerLock.release();
                } else if (encoderStatus < 0) {

                } else {
                    if (endOfStream) {
                        mAudioEncoder.releaseOutputBuffer(encoderStatus, false);
                        break;
                    }

                    ByteBuffer encodedData = encoderOutputBuffers[encoderStatus];
                    if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                        bufferInfo.size = 0;
                    }

                    if (bufferInfo.size != 0 && mMuxerStarted &&
                            (oldTimeStampUs < (bufferInfo.presentationTimeUs))) {
                        encodedData.position(bufferInfo.offset);
                        encodedData.limit(bufferInfo.offset + bufferInfo.size);
                        mMuxer.writeSampleData(mAudioTrackIndex, encodedData, bufferInfo);
                        oldTimeStampUs = bufferInfo.presentationTimeUs;
                    }
                    mAudioEncoder.releaseOutputBuffer(encoderStatus, false);

                    if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                        break;
                    }
                }
            }
            releaseAudioEncoder();
        }

    }

    class AudioRecorderThread extends Thread {
        boolean endOfStream;

        public AudioRecorderThread(boolean endOfStream) {
            this.endOfStream = endOfStream;
        }

        @Override
        public void run() {
            mAudioRecorderRunning = true;
            ByteBuffer[] encoderInputBuffers = mAudioEncoder.getInputBuffers();
            while (mAudioRecorderRunning) {
                int bufferIndex = mAudioEncoder.dequeueInputBuffer(TIMEOUT_USEC);
                if (bufferIndex >= 0) {
                    ByteBuffer inputBuffer = encoderInputBuffers[bufferIndex];
                    inputBuffer.clear();
                    AudioTimestamp audioTS = new AudioTimestamp();
                    int inputLen = 0;
                    inputLen = mAudioRecord.read(inputBuffer, AUDIO_SAMPLES_PER_FRAME,
                            AudioRecord.READ_BLOCKING);
                    int status = mAudioRecord.getTimestamp(audioTS,
                            AudioTimestamp.TIMEBASE_MONOTONIC);
                    if (status != AudioRecord.SUCCESS) {
                        Log.e(TAG, "Invalid Audio TimeStamp");
                    }
                    mPresentationTimeUs = audioTS.nanoTime / 1000;

                    if (mPresentationTimeUs != -1L) {
                        if (endOfStream) {
                            mAudioEncoder.queueInputBuffer(bufferIndex, 0, inputLen,
                                    mPresentationTimeUs, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
                        } else {
                            mAudioEncoder.queueInputBuffer(bufferIndex, 0, inputLen,
                                    mPresentationTimeUs, 0);
                        }
                    }
                }
            }
            mAudioRecord.stop();
            mAudioEncoderRunning = false;
        }
    }
}