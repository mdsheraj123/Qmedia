/*
Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package org.codeaurora.qmedia;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.ImageFormat;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.util.Log;
import android.widget.ImageView;
import android.widget.Toast;

import com.qualcomm.qti.snpe.FloatTensor;
import com.qualcomm.qti.snpe.NeuralNetwork;
import com.qualcomm.qti.snpe.SNPE;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

public class SnpeBase {

    private static final String TAG = "SnpeBase";
    private static final String MODEL_FILE_PATH = "/storage/emulated/0/DCIM/SnpeModel/";
    private static final String SNPE_MODEL_ASSET_NAME = "qseg_person_align_1_quant.dlc";
    private static final String SNPE_MODEL_INPUT_LAYER = "actual_input_1";
    private static final String SNPE_MODEL_OUTPUT_LAYER = "output1";
    private static final int SNPE_MODEL_INPUT_CHANNEL_COUNT = 3;
    private final Activity mActivity;
    private final Context mContext;
    private final Application mApplication;
    private ImageView mOutputImageView = null;
    private CameraBase mCameraBase = null;
    private int mFrameCount = 0;
    private int mPreviewFrameCount = 0;
    private int mInferenceFrameCount = 0;
    private long mInitialTime = -1;
    private long mPreviewInitialTime = -1;
    private long mInferenceInitialTime = -1;
    private Image mLatestImage = null;
    private final Object mLatestImageSyncObject = new Object();
    private final Object mModelInputFloatArraySyncObject = new Object();
    private final Object mFloatInferenceOutputArraySyncObject = new Object();
    private ImageReader mYUVImageReader = null;
    private HandlerThread mImageListenerThread = null;
    private Handler mImageListenerHandler = null;
    private final AtomicBoolean mCameraRunning = new AtomicBoolean(false);
    private final AtomicBoolean mMLRunning = new AtomicBoolean(false);
    private int mCameraWidth = 1280;
    private int mCameraHeight = 720;

    private Bitmap mCameraInputBitmap;
    private Bitmap mFinalOutputBitmap;
    private float[] mModelInputFloatArray = null;
    private float[] mFloatInferenceOutputArray = null;
    private boolean mNetworkLoaded;
    private int[] mInputTensorShape = null;
    private Map<String, FloatTensor> mInputTensorsMap = null;
    private FloatTensor mInputTensor = null;
    private Map<String, FloatTensor> mOutputTensorsMap = null;
    private NeuralNetwork mNeuralNetwork = null;
    private Thread mMlPreAndPostProcessingThread = null;
    private Thread mMlInferenceThread = null;
    private RenderScriptProcessing mRenderScriptProcessing;
    private String mSnpeRuntime;

    public SnpeBase(Activity activity, String snpeRuntime, int cameraWidth, int cameraHeight) {
        Log.d(TAG, "SnpeBase setup with runtime=" + snpeRuntime + " width=" + cameraWidth + " height=" + cameraHeight);
        mActivity = activity;
        mContext = activity.getApplicationContext();
        mApplication = activity.getApplication();
        mSnpeRuntime = snpeRuntime;
        mCameraWidth = cameraWidth;
        mCameraHeight = cameraHeight;

        mNetworkLoaded = loadSnpeFromAssets();
        mModelInputFloatArray = new float[getInputTensorWidth() * getInputTensorHeight() * SNPE_MODEL_INPUT_CHANNEL_COUNT];
        mCameraInputBitmap = Bitmap.createBitmap(mCameraWidth, mCameraHeight, Bitmap.Config.ARGB_8888);
        mFinalOutputBitmap = Bitmap.createBitmap(mCameraWidth, mCameraHeight, Bitmap.Config.ARGB_8888);

        mRenderScriptProcessing = new RenderScriptProcessing(mContext, this, mLatestImageSyncObject,
                mModelInputFloatArraySyncObject, mFloatInferenceOutputArraySyncObject, mCameraInputBitmap,
                mCameraWidth, mCameraHeight, getInputTensorWidth(), getInputTensorHeight(),
                mFinalOutputBitmap, 25.0f);
    }

    private void startBackgroundThreads() {
        mImageListenerThread = new HandlerThread("ImageThread");
        mImageListenerThread.start();
        mImageListenerHandler = new Handler(mImageListenerThread.getLooper());
    }

    private void stopBackgroundThreads() {
        mImageListenerThread.quitSafely();
        try {
            mImageListenerThread.join();
            mImageListenerThread = null;
            mImageListenerHandler = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    public void addPreviewImageView(ImageView outputImageView) {
        mOutputImageView = outputImageView;
    }

    @SuppressLint("MissingPermission")
    public void startInference(String id) {
        startBackgroundThreads();

        mYUVImageReader = ImageReader.newInstance(mCameraWidth, mCameraHeight, ImageFormat.YUV_420_888, 8);
        mYUVImageReader.setOnImageAvailableListener(mYUVImageReaderListener, mImageListenerHandler);

        if (!mCameraRunning.getAndSet(true)) {
            mCameraBase = new CameraBase(mContext, null);
            mCameraBase.addMLImageSurface(mYUVImageReader.getSurface());
            mCameraBase.startCamera(id);
        }

        mMLRunning.set(true);
        mMlPreAndPostProcessingThread = new MlPreAndPostProcessingThread();
        mMlPreAndPostProcessingThread.start();
        mMlInferenceThread = new MlInferenceThread();
        mMlInferenceThread.start();
    }

    public void stopInference() {
        mMLRunning.set(false);
        try {
            mMlInferenceThread.join();
            mMlInferenceThread = null;
            mMlPreAndPostProcessingThread.join();
            mMlPreAndPostProcessingThread = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        if (mCameraBase != null && mCameraRunning.getAndSet(false)) {
            mCameraBase.stopCamera();
            mCameraBase = null;
        }
        if (mYUVImageReader != null) {
            mYUVImageReader.close();
            mYUVImageReader = null;
        }
        if (mLatestImage != null) {
            mLatestImage.close();
            mLatestImage = null;
        }
        stopBackgroundThreads();
    }

    public Image getLatestImage() {
        return mLatestImage;
    }

    public float[] getModelInputFloatArray() {
        return mModelInputFloatArray;
    }

    public float[] getFloatInferenceOutputArray() {
        return mFloatInferenceOutputArray;
    }

    private final ImageReader.OnImageAvailableListener mYUVImageReaderListener =
            reader -> {
                try {
                    synchronized (mLatestImageSyncObject) {
                        Image img = reader.acquireLatestImage();
                        if (img == null) {
                            Log.e(TAG, "Null image returned for YUV");
                            return;
                        }
                        if (mLatestImage != null) {
                            mLatestImage.close();
                        }
                        mLatestImage = img;
                    }
                    // Print after every 3 second for 30 fps use case.
                    mFrameCount++;
                    if (mFrameCount % 90 == 0) {
                        long currentTime = SystemClock.elapsedRealtimeNanos();
                        if (mInitialTime != -1) {
                            float fps = (float) (mFrameCount * 1e9 / (currentTime - mInitialTime));
                            Log.i(TAG,
                                    "Stream Width: " + mLatestImage.getWidth() + " Stream Height: " +
                                            mLatestImage.getHeight() +
                                            " Camera FPS: " + String.format("%.02f", fps));
                        }
                        mFrameCount = 0;
                        mInitialTime = currentTime;
                    }
                } catch (IllegalStateException e) {
                    e.printStackTrace();
                }
            };

    class MlInferenceThread extends Thread {
        @Override
        public void run() {
            while (mMLRunning.get()) {
                if (mNetworkLoaded) {

                    if (mLatestImage != null) {
                        if (!mRenderScriptProcessing.preProcess()) {
                            continue;
                        }

                        if (mFloatInferenceOutputArray != null) {
                            mRenderScriptProcessing.postProcess();

                            // Print after every 3 second for 30 fps use case.
                            mPreviewFrameCount++;
                            if (mPreviewFrameCount % 90 == 0) {
                                long currentTime = SystemClock.elapsedRealtimeNanos();
                                if (mPreviewInitialTime != -1) {
                                    float fps = (float) (mPreviewFrameCount * 1e9 / (currentTime - mPreviewInitialTime));
                                    Log.i(TAG, String.format("Preview fps is %.02f", fps));
                                }
                                mPreviewFrameCount = 0;
                                mPreviewInitialTime = currentTime;
                            }
                        }
                    }
                    mActivity.runOnUiThread(() -> {
                        mOutputImageView.setImageBitmap(mFinalOutputBitmap);
                    });
                }
            }
        }
    }

    class MlPreAndPostProcessingThread extends Thread {
        @Override
        public void run() {
            while (mMLRunning.get()) {
                if (mNetworkLoaded) {
                    if (mModelInputFloatArray != null) {
                        SnpeInference();
                    }
                }
            }
        }
    }

    private int getInputTensorWidth() {
        return mInputTensorShape == null ? 0 : mInputTensorShape[2];
    }

    private int getInputTensorHeight() {
        return mInputTensorShape == null ? 0 : mInputTensorShape[1];
    }

    private void releaseNeuralNetwork() {
        if (mInputTensorsMap != null) {
            for (Map.Entry<String, FloatTensor> entry : mInputTensorsMap.entrySet()) {
                entry.getValue().release();
            }
        }
        if (mOutputTensorsMap != null) {
            for (Map.Entry<String, FloatTensor> entry : mOutputTensorsMap.entrySet()) {
                entry.getValue().release();
            }
        }
        if (mNeuralNetwork != null) {
            mNeuralNetwork.release();
        }
        mNeuralNetwork = null;
        mInputTensorShape = null;
        mInputTensor = null;
        mInputTensorsMap = null;
        mOutputTensorsMap = null;
    }

    private boolean loadSnpeFromAssets() {
        Log.d(TAG, "loadSnpeFromAssets");
        releaseNeuralNetwork();
        NeuralNetwork.Runtime selectedCore;
        switch (mSnpeRuntime) {
            case "CPU":
                selectedCore = NeuralNetwork.Runtime.CPU;
                break;
            case "GPU":
                selectedCore = NeuralNetwork.Runtime.GPU;
                break;
            case "DSP":
                selectedCore = NeuralNetwork.Runtime.DSP;
                break;
            case "GPU_FLOAT16":
                selectedCore = NeuralNetwork.Runtime.GPU_FLOAT16;
                break;
            case "AIP":
                selectedCore = NeuralNetwork.Runtime.AIP;
                break;
            default:
                selectedCore = NeuralNetwork.Runtime.DSP;
        }
        mNeuralNetwork = loadNetworkFromDlc(mApplication, selectedCore);
        if (mNeuralNetwork == null) {
            informViaToast("Error loading the DLC network on the " + selectedCore + " core. Retrying on CPU.");
            mNeuralNetwork = loadNetworkFromDlc(mApplication, NeuralNetwork.Runtime.CPU);
            if (mNeuralNetwork == null) {
                informViaToast("Error loading the DLC network on CPU");
                return false;
            }
            informViaToast("Loading the DLC network on the CPU worked");
        }
        String mRuntimeCoreName = mNeuralNetwork.getRuntime().toString();
        Log.e(TAG, "mRuntimeCoreName is " + mRuntimeCoreName);
        mInputTensorShape = mNeuralNetwork.getInputTensorsShapes().get(SNPE_MODEL_INPUT_LAYER);
        mInputTensor = mNeuralNetwork.createFloatTensor(mInputTensorShape);
        mInputTensorsMap = new HashMap<>();
        mInputTensorsMap.put(SNPE_MODEL_INPUT_LAYER, mInputTensor);

        return true;
    }

    private static NeuralNetwork loadNetworkFromDlc(
            Application application, NeuralNetwork.Runtime selectedRuntime,
            String... outputLayerNames) {
        try {
            File dlcModelFile = new File(MODEL_FILE_PATH + SNPE_MODEL_ASSET_NAME);
            InputStream assetInputStream = new FileInputStream(dlcModelFile);

            NeuralNetwork network = new SNPE.NeuralNetworkBuilder(application)
                    .setDebugEnabled(false)
                    .setOutputLayers(outputLayerNames)
                    .setModel(assetInputStream, assetInputStream.available())
                    .setPerformanceProfile(NeuralNetwork.PerformanceProfile.HIGH_PERFORMANCE)
                    .setRuntimeOrder(selectedRuntime)
                    .setCpuFallbackEnabled(true)
                    .build();

            assetInputStream.close();
            return network;
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    private void SnpeInference() {
        try {
            synchronized (mModelInputFloatArraySyncObject) {
                mInputTensor.write(mModelInputFloatArray, 0, mModelInputFloatArray.length, 0, 0);
            }
            mOutputTensorsMap = mNeuralNetwork.execute(mInputTensorsMap);

            if (mOutputTensorsMap == null) {
                throw new Exception("SnpeInference output is null");
            }
            int outputSize = mOutputTensorsMap.get(SNPE_MODEL_OUTPUT_LAYER).getSize();
            if (mFloatInferenceOutputArray == null) {
                mFloatInferenceOutputArray = new float[outputSize];
            }
            synchronized (mFloatInferenceOutputArraySyncObject) {
                mOutputTensorsMap.get(SNPE_MODEL_OUTPUT_LAYER).read(mFloatInferenceOutputArray, 0, outputSize);
            }

            // Print after every 90 inferences.
            mInferenceFrameCount++;
            if (mInferenceFrameCount % 90 == 0) {
                long currentTime = SystemClock.elapsedRealtimeNanos();
                if (mInferenceInitialTime != -1) {
                    float fps = (float) (mInferenceFrameCount * 1e9 / (currentTime - mInferenceInitialTime));
                    Log.i(TAG, String.format("Inference fps is %.02f", fps));
                }
                mInferenceFrameCount = 0;
                mInferenceInitialTime = currentTime;
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void informViaToast(String message) {
        mActivity.runOnUiThread(() -> Toast.makeText(mContext, message, Toast.LENGTH_LONG).show());
    }

}