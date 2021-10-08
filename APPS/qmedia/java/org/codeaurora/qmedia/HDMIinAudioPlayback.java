/*
# Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted (subject to the limitations in the
# disclaimer below) provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#
#     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
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

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.util.Log;

import java.util.Arrays;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicBoolean;

public class HDMIinAudioPlayback {

    private static final String TAG = "HDMIinAudio";
    private static final int DEFAULT_SAMPLE_RATE = 48000;
    private static final int AUDIO_QUEUE_SIZE = 8;
    private int mAudioBufferBytes;
    private int mAudioSampleRate;
    private int mRecorderChannels;
    private int mRecorderAudioEncoding;
    private AudioRecord mAudioRecorder = null;
    private AudioTrack mAudioTrack = null;
    AudioManager mAudioManager;
    AudioDeviceInfo[] mAudioDeviceInfos;
    AudioDeviceInfo mHDMIDevice = null;
    private Thread mRecordThread = null;
    private Thread mPlaybackThread = null;
    ArrayBlockingQueue<byte[]> mAudioQueue = new ArrayBlockingQueue<>(AUDIO_QUEUE_SIZE);
    AtomicBoolean isAudioRecordThreadRunning = new AtomicBoolean(false);
    AtomicBoolean isAudioPlaybackThreadRunning = new AtomicBoolean(false);
    Semaphore mRecordPlaybackSemaphore = new Semaphore(2);


    public HDMIinAudioPlayback(Context context) {
        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    }

    public void start() {

        mAudioDeviceInfos = mAudioManager.getDevices(AudioManager.GET_DEVICES_INPUTS);
        for (AudioDeviceInfo d : mAudioDeviceInfos) {
            if (d.getType() == AudioDeviceInfo.TYPE_HDMI)
                mHDMIDevice = d;
        }
        if (mHDMIDevice != null) {
            int[] sampleRates = mHDMIDevice.getSampleRates();
            if (sampleRates.length == 0) {
                mAudioSampleRate = DEFAULT_SAMPLE_RATE;
            } else {
                mAudioSampleRate = Arrays.stream(sampleRates).max().getAsInt();
            }
            mRecorderChannels = Arrays.stream(mHDMIDevice.getChannelMasks()).
                    filter(it -> it == AudioFormat.CHANNEL_IN_STEREO || it == AudioFormat.CHANNEL_IN_MONO).
                    findFirst().getAsInt();
            mRecorderAudioEncoding = Arrays.stream(mHDMIDevice.getEncodings()).
                    filter(it -> it == AudioFormat.ENCODING_PCM_16BIT || it == AudioFormat.ENCODING_PCM_FLOAT).
                    findFirst().getAsInt();

            mAudioBufferBytes = AudioRecord.getMinBufferSize(mAudioSampleRate,
                    mRecorderChannels,
                    mRecorderAudioEncoding);

            Log.d(TAG, "Init HDMI audio");
            mAudioRecorder = new AudioRecord(MediaRecorder.AudioSource.UNPROCESSED,
                    mAudioSampleRate, mRecorderChannels,
                    mRecorderAudioEncoding, mAudioBufferBytes);
            mAudioRecorder.setPreferredDevice(mHDMIDevice);
            mAudioRecorder.startRecording();

            mAudioTrack = new AudioTrack.Builder()
                    .setAudioAttributes(new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_MEDIA)
                            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                            .build())
                    .setAudioFormat(new AudioFormat.Builder()
                            .setEncoding(mRecorderAudioEncoding)
                            .setSampleRate(mAudioSampleRate)
                            .setChannelMask(mRecorderChannels)
                            .build())
                    .setBufferSizeInBytes(mAudioBufferBytes)
                    .build();
            mAudioTrack.play();

            try {
                mRecordPlaybackSemaphore.acquire(2);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

            mRecordThread = new Thread(new Runnable() {
                public void run() {
                    isAudioRecordThreadRunning.set(true);
                    mRecordPlaybackSemaphore.release();
                    Log.e(TAG, "Audio Record thread started.");
                    byte[] bData = new byte[mAudioBufferBytes];
                    while (isAudioRecordThreadRunning.get()) {
                        mAudioRecorder.read(bData, 0, mAudioBufferBytes);
                        mAudioQueue.add(bData);
                    }
                }
            }, "Audio Record Thread");
            mRecordThread.start();
            mPlaybackThread = new Thread(new Runnable() {
                public void run() {
                    isAudioPlaybackThreadRunning.set(true);
                    mRecordPlaybackSemaphore.release();
                    Log.e(TAG, "Audio Playback thread started.");
                    while (isAudioPlaybackThreadRunning.get()) {
                        if (!mAudioQueue.isEmpty()) {
                            byte[] bData = mAudioQueue.remove();
                            mAudioTrack.write(bData, 0, mAudioBufferBytes);
                        }
                    }
                    mRecordPlaybackSemaphore.release();
                }
            }, "Audio Playback Thread");
            mPlaybackThread.start();
        }
    }

    public void stop() {
        if (mHDMIDevice != null) {
            try {
                mHDMIDevice = null;

                mRecordPlaybackSemaphore.acquire(2);
                mRecordPlaybackSemaphore.release(2);

                isAudioRecordThreadRunning.set(false);
                isAudioPlaybackThreadRunning.set(false);

                mRecordThread.join();
                mRecordThread = null;

                mPlaybackThread.join();
                mPlaybackThread = null;

                mAudioQueue.clear();

                if (mAudioRecorder != null) {
                    mAudioRecorder.stop();
                    mAudioRecorder.release();
                    mAudioRecorder = null;
                }

                if (mAudioTrack != null) {
                    mAudioTrack.stop();
                    mAudioTrack.release();
                    mAudioTrack = null;
                }
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }
}
