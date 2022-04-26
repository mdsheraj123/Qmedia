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

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.InputConfiguration;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.media.Image;
import android.media.ImageReader;
import android.media.ImageWriter;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.util.Range;
import android.view.PixelCopy;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.ImageView;
import android.widget.Toast;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

public class CameraBase {

    private static final String TAG = "CameraBase";
    private static final CaptureRequest.Key<Byte> BYPASS_RESOURCE_CHECK_KEY =
            new CaptureRequest.Key<>(
                    "org.codeaurora.qcamera3.sessionParameters.overrideResourceCostValidation",
                    byte.class);
    private static final CaptureRequest.Key<int[]> CHI_RECTANGLE_KEY =
            new CaptureRequest.Key<int[]>(
                    "com.qti.camera.multiROIinfo.streamROIInfo",
                    int[].class);
    private static final CaptureRequest.Key<Byte> MULTI_ROI_ENABLE_KEY =
            new CaptureRequest.Key<>(
                    "org.codeaurora.qcamera3.sessionParameters.MultiRoIEnable",
                    byte.class);
    private static final CaptureRequest.Key<Integer> STREAM_ROI_COUNT_KEY =
            new CaptureRequest.Key<Integer>(
                    "com.qti.camera.multiROIinfo.streamROICount",
                    Integer.class);
    private static final int CHANGE_ROI_DATA_NTH_FRAME = 300;
    private final Context mCameraContext;
    private final Semaphore mCameraOpenCloseLock = new Semaphore(1);
    private HandlerThread mBackgroundThread;
    private Handler mBackgroundHandler;
    private CameraDevice mCameraDevice;
    private CameraCaptureSession mCaptureSession;
    private CaptureRequest.Builder mPreviewRequestBuilder;
    private SurfaceHolder mStreamSurfaceHolder = null;
    private Surface mRecordSurface;
    private Boolean mRecord = false;
    private final CameraDisconnectedListener mCameraDisconnectedListener;
    private Boolean mEnableReproc = false;
    private final ArrayList<SurfaceView> mSurfaceViewList = new ArrayList<>();
    private ImageReader mYUVImageReader = null;
    private HandlerThread mImageListenerThread = null;
    private Handler mImageListenerHandler;
    private ImageWriter mImageWriter = null;
    private final AtomicBoolean mCameraIsRunning = new AtomicBoolean(false);
    private final ArrayBlockingQueue<Image> mYuvImageQueue = new ArrayBlockingQueue<Image>(8);
    private TotalCaptureResult mLastTotalCaptureResult;
    private Thread mCameraReprocThread;
    private ImageView mImageView;
    private Bitmap mImageViewBitmap = null;
    private int[] mROIDataSetOne = {0, 0, 1920, 1080, 1920, 0, 1920, 1080, 0, 1080, 1920, 1080};
    private int[] mROIDataSetTwo = {1920, 0, 1920, 1080, 0, 1080, 1920, 1080, 0, 0, 1920, 1080};
    private int mFrameNumber = 0;
    private int mFrameCount = 0;
    private long mInitialTime;
    private Range<Integer> mFPSRange = new Range(30, 30);

    public CameraBase(Context context, CameraDisconnectedListener cameraDisconnectedListener) {
        mCameraContext = context;
        mCameraDisconnectedListener = cameraDisconnectedListener;
    }

    // Reprocessing capture completed.
    private CameraCaptureSession.CaptureCallback mReprocessingCaptureCallback =
            new CameraCaptureSession.CaptureCallback() {
                @Override
                public void onCaptureCompleted(CameraCaptureSession session, CaptureRequest request,
                                               TotalCaptureResult result) {
                }
            };

    private CameraCaptureSession.CaptureCallback mCaptureCallback =
            new CameraCaptureSession.CaptureCallback() {
                @Override
                public void onCaptureCompleted(@NonNull CameraCaptureSession session,
                                               @NonNull CaptureRequest request,
                                               @NonNull TotalCaptureResult result) {
                    mLastTotalCaptureResult = result;
                }
            };


    private final ImageReader.OnImageAvailableListener mYUVImageReaderListener =
            reader -> {
                try {
                    Image img = reader.acquireNextImage();
                    if (img == null) {
                        Log.e(TAG, "Null image returned YUV");
                        return;
                    }
                    mYuvImageQueue.add(img);
                    mFrameCount++;
                    // Print after every 3 second for 30 fps use case.
                    if (mFrameCount % 90 == 0) {
                        long currentTime = SystemClock.elapsedRealtimeNanos();
                        float fps = (float) (mFrameCount * 1e9 / (currentTime - mInitialTime));
                        Log.i(TAG,
                                "Stream Width: " + img.getWidth() + " Stream Height: " +
                                        img.getHeight() +
                                        " Camera FPS: " + String.format("%.02f", fps));
                        mFrameCount = 0;
                        mInitialTime = SystemClock.elapsedRealtimeNanos();
                    }
                } catch (IllegalStateException e) {
                    e.printStackTrace();
                }

            };
    private final CameraDevice.StateCallback mStateCallback = new CameraDevice.StateCallback() {

        @Override
        public void onOpened(CameraDevice cameraDevice) {
            // This method is called when the camera is opened. We start camera preview here.
            Log.v(TAG, "onOpened Camera id: " + cameraDevice.getId());
            mCameraDevice = cameraDevice;
            createAndStartCameraSession();
        }

        @Override
        public void onDisconnected(CameraDevice cameraDevice) {
            Log.v(TAG, "onDisconnected Called for camera # " + cameraDevice.getId());
            mCameraDisconnectedListener.cameraDisconnected();
        }

        @Override
        public void onError(CameraDevice cameraDevice, int error) {
            Log.e(TAG, "Error" + error + " occurred on camera ID : " + cameraDevice.getId());
            mCameraOpenCloseLock.release();
            cameraDevice.close();
            mCameraDevice = null;
        }

        @Override
        public void onClosed(CameraDevice camera) {
            Log.v(TAG, "onClosed Called for camera # " + camera.getId());
            super.onClosed(camera);
            mCameraOpenCloseLock.release();
        }
    };

    private final CameraCaptureSession.StateCallback mCaptureStateCallBack =
            new CameraCaptureSession.StateCallback() {

                @Override
                public void onConfigured(
                        @NonNull CameraCaptureSession cameraCaptureSession) {
                    Log.v(TAG, "onConfigured");
                    mCameraOpenCloseLock.release();
                    // The camera is already closed
                    if (null == mCameraDevice) {
                        Log.w(TAG, "mCameraDevice is null, returning from onConfigured");
                        return;
                    }
                    mCaptureSession = cameraCaptureSession;
                    try {
                        // Set Default Params
                        setDefaultCameraParam();
                        if (mEnableReproc) {
                            if (mCaptureSession.isReprocessable()) {
                                if (mImageWriter != null) {
                                    mImageWriter.close();
                                    mImageWriter = null;
                                }
                                mImageWriter = ImageWriter
                                        .newInstance(mCaptureSession.getInputSurface(), 4);
                                mImageWriter.setOnImageReleasedListener(
                                        writer -> Log.v(TAG, "ImageWriter.OnImageReleasedListener onImageReleased()"),
                                        null);
                                Log.v(TAG, "Created ImageWriter.");
                            }
                        }

                        // Finally, we start displaying the camera preview.
                        CaptureRequest mPreviewRequest = mPreviewRequestBuilder.build();
                        mCaptureSession.setRepeatingRequest(mPreviewRequest,
                                mCaptureCallback, mBackgroundHandler);
                    } catch (CameraAccessException e) {
                        e.printStackTrace();
                    }
                }

                @Override
                public void onConfigureFailed(
                        @NonNull CameraCaptureSession cameraCaptureSession) {
                    Log.e(TAG, "onConfigureFailed");
                    mCameraOpenCloseLock.release();
                    Toast.makeText(mCameraContext.getApplicationContext(), "onConfigureFailed",
                            Toast.LENGTH_SHORT).show();
                }
            };

    private void startBackgroundThread() {
        Log.v(TAG, "startBackgroundThread enter");
        mBackgroundThread = new HandlerThread("Camera2BackgroundThread");
        mBackgroundThread.start();
        mBackgroundHandler = new Handler(mBackgroundThread.getLooper());
        if (mEnableReproc) {
            mImageListenerThread = new HandlerThread("ImageThread");
            mImageListenerThread.start();
            mImageListenerHandler = new Handler(mImageListenerThread.getLooper());
        }
        Log.v(TAG, "startBackgroundThread exit");
    }

    private void stopBackgroundThread() {
        Log.v(TAG, "stopBackgroundThread enter");
        mBackgroundThread.quitSafely();
        try {
            mBackgroundThread.join();
            mBackgroundThread = null;
            mBackgroundHandler = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        if (mEnableReproc) {
            mImageListenerThread.quitSafely();
            try {
                mImageListenerThread.join();
                mImageListenerThread = null;
                mImageListenerHandler = null;
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            if (mYUVImageReader != null) {
                mYUVImageReader.close();
                mYUVImageReader = null;
            }
        }
        Log.v(TAG, "stopBackgroundThread exit");
    }

    @SuppressLint("MissingPermission")
    public void startCamera(String id) {
        Log.v(TAG, "startCamera enter");
        Log.v(TAG, "Opening Camera ID" + id);
        mInitialTime = SystemClock.elapsedRealtimeNanos();
        CameraManager manager =
                (CameraManager) mCameraContext.getSystemService(Context.CAMERA_SERVICE);
        try {
            mCameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS);
            startBackgroundThread();
            manager.openCamera(id, mStateCallback, mBackgroundHandler);
            mCameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS);
            if (mEnableReproc) {
                mImageViewBitmap = Bitmap.createBitmap(3840, 2160, Bitmap.Config.ARGB_8888);
                mCameraReprocThread = new CameraReprocThread();
                mCameraIsRunning.set(true);
                mCameraReprocThread.start();
            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
        } catch (InterruptedException e) {
            throw new RuntimeException("Interrupted while trying to lock camera opening.", e);
        } finally {
            mCameraOpenCloseLock.release();
        }
        Log.v(TAG, "startCamera exit");
    }

    public void stopCamera() {
        Log.v(TAG, "stopCamera enter");
        mFrameCount = 0;
        mFrameNumber = 0;
        if (mCameraDevice != null) {
            Log.v(TAG, "Closing Camera ID # " + mCameraDevice.getId());
        }
        if (mEnableReproc) {
            mCameraIsRunning.set(false);
            try {
                mCameraReprocThread.join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            mCameraReprocThread = null;
            mImageViewBitmap = null;
        }
        // Clear the surface
        if (mStreamSurfaceHolder != null) {
            mStreamSurfaceHolder.setFormat(PixelFormat.TRANSPARENT);
            mStreamSurfaceHolder.setFormat(PixelFormat.OPAQUE);
        }

        try {
            mCameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS);
            if (null != mCaptureSession) {
                mCaptureSession.close();
                mCaptureSession = null;
            }
            if (null != mCameraDevice) {
                mCameraDevice.close();
                mCameraDevice = null;
            }
            mCameraOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            throw new RuntimeException("Interrupted while trying to lock camera closing.", e);
        } finally {
            mCameraOpenCloseLock.release();
        }
        stopBackgroundThread();
        Log.v(TAG, "stopCamera exit");
    }

    public void addPreviewStream(SurfaceHolder surface) {
        mStreamSurfaceHolder = surface;
    }

    public void enableReproc(ImageView view) {
        mEnableReproc = true;
        mImageView = view;
        if (mYUVImageReader == null) {
            mYUVImageReader = ImageReader.newInstance(3840, 2160,
                    ImageFormat.YUV_420_888, 8);
            mYUVImageReader
                    .setOnImageAvailableListener(mYUVImageReaderListener, mImageListenerHandler);
        }
    }

    public void addReprocStream(SurfaceView surface) {
        mSurfaceViewList.add(surface);
    }

    private void createAndStartCameraSession() {
        Log.v(TAG, "createAndStartCameraSession enter");
        try {
            mPreviewRequestBuilder
                    = mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, mFPSRange);
            try {
                mPreviewRequestBuilder.set(BYPASS_RESOURCE_CHECK_KEY, (byte) 0x01);
            } catch (IllegalArgumentException e) {
                Log.w(TAG, "Resource ByPass Key does not exist");
            }
            List<Surface> outputs = new ArrayList<>();
            if (mStreamSurfaceHolder != null) {
                mPreviewRequestBuilder.addTarget(mStreamSurfaceHolder.getSurface());
                outputs.add(mStreamSurfaceHolder.getSurface());
            }
            if (mEnableReproc) {
                if (mYUVImageReader == null) {
                    mYUVImageReader = ImageReader.newInstance(3840, 2160, ImageFormat.YUV_420_888, 8);
                    mYUVImageReader
                            .setOnImageAvailableListener(mYUVImageReaderListener, mImageListenerHandler);
                }
                mPreviewRequestBuilder.addTarget(mYUVImageReader.getSurface());
                outputs.add(mYUVImageReader.getSurface());
                try {
                    mPreviewRequestBuilder.set(MULTI_ROI_ENABLE_KEY, (byte) 0x01);
                } catch (IllegalArgumentException e) {
                    Log.w(TAG, "Multi ROI Enable Key does not exist");
                }
            }
            if (mRecord && mRecordSurface != null) {
                mPreviewRequestBuilder.addTarget(mRecordSurface);
                outputs.add(mRecordSurface);
            }
            if (!mSurfaceViewList.isEmpty()) {
                for (SurfaceView view : mSurfaceViewList) {
                    outputs.add(view.getHolder().getSurface());
                }
            }
            List<OutputConfiguration> outConfigurations = new ArrayList<>(outputs.size());
            for (Surface obj : outputs) {
                outConfigurations.add(new OutputConfiguration(obj));
            }

            SessionConfiguration sessionCfg = new SessionConfiguration(
                    SessionConfiguration.SESSION_REGULAR,
                    outConfigurations,
                    new HandlerExecutor(mBackgroundHandler),
                    mCaptureStateCallBack);

            sessionCfg.setSessionParameters(mPreviewRequestBuilder.build());
            if (mEnableReproc) {
                InputConfiguration inputConfig = new InputConfiguration(3840,2160,
                        ImageFormat.YUV_420_888);
                sessionCfg.setInputConfiguration(inputConfig);
            }
            mCameraDevice.createCaptureSession(sessionCfg);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
        Log.v(TAG, "createAndStartCameraSession exit");
    }

    private void setDefaultCameraParam() {
        Log.v(TAG, "setDefaultCameraParam enter");
        mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE,
                CaptureRequest.CONTROL_AF_MODE_OFF);
        mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AE_ANTIBANDING_MODE,
                CaptureRequest.CONTROL_AE_ANTIBANDING_MODE_AUTO);
        mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AE_EXPOSURE_COMPENSATION, 0);
        mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AE_MODE,
                CaptureRequest.CONTROL_AE_MODE_ON);
        mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AE_LOCK, false);
        mPreviewRequestBuilder
                .set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_AUTO);
        mPreviewRequestBuilder.set(CaptureRequest.NOISE_REDUCTION_MODE, 1);
        Log.v(TAG, "setDefaultCameraParam exit");
    }

    public void addRecorderStream(Surface recorderSurface) {
        mRecordSurface = recorderSurface;
        mRecord = true;
    }

    class CameraReprocThread extends Thread {
        @Override
        public void run() {
            while (true) {
                Image image = mYUVImageReader.acquireNextImage();
                if (image == null) {
                    break;
                }
                image.close();
            }
            mYuvImageQueue.clear();
            while (mCameraIsRunning.get()) {
                if (!mYuvImageQueue.isEmpty()) {
                    PixelCopy.request(mYUVImageReader.getSurface(), mImageViewBitmap, i -> {
                        mImageView.setImageBitmap(mImageViewBitmap);
                    }, new Handler(Looper.getMainLooper()));
                    Image img = mYuvImageQueue.remove();
                    mImageWriter.queueInputImage(img);
                    try {
                        CaptureRequest.Builder builder =
                                mCameraDevice
                                        .createReprocessCaptureRequest(mLastTotalCaptureResult);
                        try {
                            mFrameNumber++;
                            if (mFrameNumber <= CHANGE_ROI_DATA_NTH_FRAME) {
                                builder.set(CHI_RECTANGLE_KEY, mROIDataSetOne);
                            } else if (mFrameNumber <= 2 * CHANGE_ROI_DATA_NTH_FRAME) {
                                builder.set(CHI_RECTANGLE_KEY, mROIDataSetTwo);
                            } else {
                                mFrameNumber = 0;
                                builder.set(CHI_RECTANGLE_KEY, mROIDataSetOne);
                            }
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        try {
                            builder.set(STREAM_ROI_COUNT_KEY, 3);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                        for (SurfaceView obj : mSurfaceViewList) {
                            builder.addTarget(obj.getHolder().getSurface());
                        }
                        mCaptureSession.capture(builder.build(), mReprocessingCaptureCallback,
                                mBackgroundHandler);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                    img.close();
                }
            }
        }
    }
}

class HandlerExecutor implements Executor {
    private final Handler mHandler;

    public HandlerExecutor(Handler handler) {
        mHandler = handler;
    }

    @Override
    public void execute(Runnable runCmd) {
        mHandler.post(runCmd);
    }
}