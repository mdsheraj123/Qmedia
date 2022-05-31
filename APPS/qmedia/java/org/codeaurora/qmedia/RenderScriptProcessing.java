/*
# Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
import android.graphics.Bitmap;
import android.media.Image;
import android.renderscript.Allocation;
import android.renderscript.Element;
import android.renderscript.RenderScript;
import android.renderscript.Script;
import android.renderscript.ScriptIntrinsicBlur;
import android.renderscript.Type;

import java.nio.ByteBuffer;

public class RenderScriptProcessing {

    SnpeBase mSnpeBase;
    RenderScript rs;
    ScriptC_preProcess preProcessScript;
    ScriptIntrinsicBlur blurScript;
    ScriptC_postProcess postProcessScript;

    Allocation yAllocation = null;
    Allocation uAllocation = null;
    Allocation vAllocation = null;
    Allocation floatOutputAllocation = null;
    Allocation outBitmapAllocation = null;

    Allocation inputBitmapAllocation = null;
    Allocation blurredBitmapAllocation = null;
    Allocation inferenceMaskAllocation = null;
    Allocation finalBitmapAllocation = null;

    private final Object mLatestImageSyncObject;
    private final Object mModelInputFloatArraySyncObject;
    private final Object mFloatInferenceOutputArraySyncObject;
    private long mLastImageTimeStamp;

    private byte[] mY = null, mU = null, mV = null;
    private Type.Builder mYType = null, mUvType = null;
    private Script.LaunchOptions mPreProcessLaunchOptions = null;


    private Bitmap mCameraInputBitmap;
    private Bitmap mFinalOutputBitmap;
    private int mCameraWidth = 1280;
    private int mCameraHeight = 720;
    private int mTensorWidth;
    private int mTensorHeight;
    private float mBlurRadius;


    RenderScriptProcessing(Context context, SnpeBase snpeBase, Object latestImageSyncObject,
                           Object modelInputFloatArraySyncObject, Object floatInferenceOutputArraySyncObject,
                           Bitmap cameraInputBitmap, int cameraWidth, int cameraHeight, int tensorWidth, int tensorHeight,
                           Bitmap finalOutputBitmap, float blurRadius) {
        mSnpeBase = snpeBase;
        rs = RenderScript.create(context);
        preProcessScript = new ScriptC_preProcess(rs);
        blurScript = ScriptIntrinsicBlur.create(rs, Element.U8_4(rs));
        postProcessScript = new ScriptC_postProcess(rs);

        mLatestImageSyncObject = latestImageSyncObject;
        mModelInputFloatArraySyncObject = modelInputFloatArraySyncObject;
        mFloatInferenceOutputArraySyncObject = floatInferenceOutputArraySyncObject;

        mCameraInputBitmap = cameraInputBitmap;
        mCameraWidth = cameraWidth;
        mCameraHeight = cameraHeight;
        mTensorWidth = tensorWidth;
        mTensorHeight = tensorHeight;
        mFinalOutputBitmap = finalOutputBitmap;
        mBlurRadius = blurRadius;

        // PreProcess
        preProcessScript.set_previewWidth(mCameraWidth);
        preProcessScript.set_previewHeight(mCameraHeight);
        preProcessScript.set_tensorWidth(mTensorWidth);
        preProcessScript.set_tensorHeight(mTensorHeight);
        preProcessScript.set_picWidth(mCameraWidth);
        Type floatOutputType = Type.createX(rs, Element.F32(rs), mTensorWidth * mTensorHeight * 3);
        floatOutputAllocation = Allocation.createTyped(rs, floatOutputType);
        preProcessScript.set_floatOutput(floatOutputAllocation);
        mPreProcessLaunchOptions = new Script.LaunchOptions();

        //PostProcess
        inputBitmapAllocation = Allocation.createFromBitmap(rs, mCameraInputBitmap);
        blurredBitmapAllocation = Allocation.createTyped(rs, inputBitmapAllocation.getType());
        postProcessScript.set_blurredBitmap(blurredBitmapAllocation);
        blurScript.setRadius(mBlurRadius); // Max is 25
        blurScript.setInput(inputBitmapAllocation);
        Type inferenceType = Type.createX(rs, Element.F32(rs), mTensorWidth * mTensorHeight);
        inferenceMaskAllocation = Allocation.createTyped(rs, inferenceType);
        postProcessScript.set_inferenceMask(inferenceMaskAllocation);
        finalBitmapAllocation = Allocation.createTyped(rs, inputBitmapAllocation.getType());
        postProcessScript.set_previewWidth(mCameraWidth);
        postProcessScript.set_previewHeight(mCameraHeight);
        postProcessScript.set_tensorWidth(mTensorWidth);
        postProcessScript.set_tensorHeight(mTensorHeight);
    }

    @Override
    protected void finalize() throws Throwable {
        rs.destroy();
        preProcessScript.destroy();
        blurScript.destroy();
        postProcessScript.destroy();

        yAllocation.destroy();
        yAllocation = null;
        uAllocation.destroy();
        uAllocation = null;
        vAllocation.destroy();
        vAllocation = null;
        floatOutputAllocation.destroy();
        floatOutputAllocation = null;
        outBitmapAllocation.destroy();
        outBitmapAllocation = null;

        inputBitmapAllocation.destroy();
        inputBitmapAllocation = null;
        blurredBitmapAllocation.destroy();
        blurredBitmapAllocation = null;
        inferenceMaskAllocation.destroy();
        inferenceMaskAllocation = null;
        finalBitmapAllocation.destroy();
        finalBitmapAllocation = null;
        super.finalize();
    }

    public boolean preProcess() {
        int yRowStride;
        int uvRowStride;
        int uvPixelStride;

        synchronized (mLatestImageSyncObject) {
            Image image = mSnpeBase.getLatestImage();
            if (image.getTimestamp() == mLastImageTimeStamp) {
                return false;
            }
            mLastImageTimeStamp = image.getTimestamp();
            Image.Plane[] planes = image.getPlanes();
            ByteBuffer buffer = planes[0].getBuffer();
            if (mY == null) {
                mY = new byte[buffer.remaining()];
            }
            buffer.get(mY);

            buffer = planes[1].getBuffer();
            if (mU == null) {
                mU = new byte[buffer.remaining()];
            }
            buffer.get(mU);

            buffer = planes[2].getBuffer();
            if (mV == null) {
                mV = new byte[buffer.remaining()];
            }
            buffer.get(mV);

            yRowStride = planes[0].getRowStride();
            uvRowStride = planes[1].getRowStride();
            uvPixelStride = planes[1].getPixelStride();
        }

        if (mYType == null) {
            mYType = new Type.Builder(rs, Element.U8(rs));
        }
        mYType.setX(yRowStride).setY(mY.length / yRowStride);

        if (yAllocation == null) {
            yAllocation = Allocation.createTyped(rs, mYType.create());
            preProcessScript.set_yIn(yAllocation);
        }
        yAllocation.copyFrom(mY);

        if (mUvType == null) {
            mUvType = new Type.Builder(rs, Element.U8(rs));
        }
        mUvType.setX(mU.length);

        if (uAllocation == null) {
            uAllocation = Allocation.createTyped(rs, mUvType.create());
            preProcessScript.set_uIn(uAllocation);
        }
        uAllocation.copyFrom(mU);

        if (vAllocation == null) {
            vAllocation = Allocation.createTyped(rs, mUvType.create());
            preProcessScript.set_vIn(vAllocation);
        }
        vAllocation.copyFrom(mV);

        preProcessScript.set_uvRowStride(uvRowStride);
        preProcessScript.set_uvPixelStride(uvPixelStride);

        if (outBitmapAllocation == null) {
            outBitmapAllocation = Allocation.createFromBitmap(rs, mCameraInputBitmap, Allocation.MipmapControl.MIPMAP_NONE, Allocation.USAGE_SCRIPT);
        }

        mPreProcessLaunchOptions.setX(0, mCameraWidth);
        mPreProcessLaunchOptions.setY(0, mY.length / yRowStride);

        preProcessScript.forEach_doConvert(outBitmapAllocation, mPreProcessLaunchOptions);
        synchronized (mModelInputFloatArraySyncObject) {
            floatOutputAllocation.copyTo(mSnpeBase.getModelInputFloatArray());
        }
        outBitmapAllocation.copyTo(mCameraInputBitmap);
        return true;
    }

    public void postProcess() {
        blurScript.forEach(blurredBitmapAllocation);
        synchronized (mFloatInferenceOutputArraySyncObject) {
            inferenceMaskAllocation.copyFrom(mSnpeBase.getFloatInferenceOutputArray());
        }
        postProcessScript.forEach_root(inputBitmapAllocation, finalBitmapAllocation);
        finalBitmapAllocation.copyTo(mFinalOutputBitmap);
    }
}
