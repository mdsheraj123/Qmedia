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

package org.codeaurora.qmedia.opengles;

import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLExt;
import android.opengl.EGLSurface;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.opengl.Matrix;
import android.util.Log;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;


class EglCore {
    private final String TAG = this.getClass().getSimpleName();
    private final EGLConfig mEglConfig;
    private EGLDisplay mEglDisplay = EGL14.EGL_NO_DISPLAY;
    private EGLContext mEglContext = EGL14.EGL_NO_CONTEXT;

    public EglCore() {
        mEglDisplay = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        if (mEglDisplay == EGL14.EGL_NO_DISPLAY) {
            try {
                throw new Exception("unable to get EGL14 display");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        int[] version = new int[2];
        if (!EGL14.eglInitialize(mEglDisplay, version, 0, version, 1)) {
            try {
                throw new Exception("unable to initialize EGL14");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        int[] attribList = {
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_ALPHA_SIZE, 8,
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                EGL14.EGL_NONE, 0,
                EGL14.EGL_NONE
        };
        EGLConfig[] configs = new EGLConfig[1];
        int[] numConfigs = new int[1];

        if (!EGL14.eglChooseConfig(this.mEglDisplay, attribList, 0, configs, 0, configs.length,
                numConfigs, 0)) {
            try {
                throw new Exception("unable to find ES2 EGL config");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        this.mEglConfig = configs[0];
        int[] attributes = new int[]{EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE};

        mEglContext = EGL14.eglCreateContext(
                mEglDisplay, configs[0], EGL14.EGL_NO_CONTEXT,
                attributes, 0
        );

        int error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            try {
                throw new Exception("eglCreateContext: EGL error: " + error);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public final void makeCurrent(EGLSurface eglSurface) {
        if (this.mEglDisplay == EGL14.EGL_NO_DISPLAY) {
            Log.d(TAG, "makeCurrent without display");
        }

        if (!EGL14.eglMakeCurrent(this.mEglDisplay, eglSurface, eglSurface, this.mEglContext)) {
            try {
                throw new Exception("eglMakeCurrent failed");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    public final boolean swapBuffers(EGLSurface eglSurface) {
        return EGL14.eglSwapBuffers(this.mEglDisplay, eglSurface);
    }

    public final void setPresentationTime(EGLSurface eglSurface, long timestamp) {
        EGLExt.eglPresentationTimeANDROID(this.mEglDisplay, eglSurface, timestamp);
    }

    public final EGLSurface createWindowSurface(Object surface) {
        if (!(surface instanceof Surface) && !(surface instanceof SurfaceTexture)) {
            try {
                throw new Exception("invalid surface: " + surface);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        int[] surfaceAttributes = new int[]{EGL14.EGL_NONE};
        EGLSurface eglSurface =
                EGL14.eglCreateWindowSurface(this.mEglDisplay, this.mEglConfig, surface,
                        surfaceAttributes, 0);
        this.checkEglError("eglCreateWindowSurface");
        if (eglSurface == null) {
            try {
                throw new Exception("surface was null");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return eglSurface;
    }


    private void checkEglError(String msg) {
        int error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            try {
                throw new Exception(msg + ": EGL error: 0x" + Integer.toHexString(error));
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

}


class CompositionRenderer {
    private static final int FLOAT_SIZE_BYTES = 4;
    private static final int VERTICES_DATA_STRIDE_BYTES = 5 * FLOAT_SIZE_BYTES;
    private static final int VERTICES_DATA_POS_OFFSET = 0;
    private static final int VERTICES_DATA_UV_OFFSET = 3;
    private static final float[] verticesData = new float[]
            {-1.0F, -1.0F, 0.0F, 0.0F, 0.0F,
                    1.0F, -1.0F, 0.0F, 1.0F, 0.0F,
                    -1.0F, 1.0F, 0.0F, 0.0F, 1.0F,
                    1.0F, 1.0F, 0.0F, 1.0F, 1.0F};
    private static final String VERTEX_SHADER =
            "uniform mat4 mvpMatrix;" +
                    "uniform mat4 stMatrix;" +
                    "attribute vec4 position;" +
                    "attribute vec4 textureCoord;" +
                    "varying vec2 vTexCoord;" +
                    "void main() {" +
                    "gl_Position = mvpMatrix * position;" +
                    "vTexCoord = (stMatrix * textureCoord).xy;" +
                    "}";
    private static final String FRAGMENT_SHADER =
            "#extension GL_OES_EGL_image_external : require\n" +
                    "precision mediump float;" +
                    "varying vec2 vTexCoord;" +
                    "uniform samplerExternalOES surfaceTexture;" +
                    "void main() {" +
                    "gl_FragColor = texture2D(surfaceTexture, vTexCoord);" +
                    "}";
    private final FloatBuffer mCompositionVertices;
    private final float[] mMVPMatrix;
    private final float[] mSTMatrix;
    private final int mProgram;
    private final int[] mTextureID;
    private final int mInstances;
    private final String TAG = this.getClass().getSimpleName();
    private int mMVPMatrixHandle = 0;
    private int mSTMatrixHandle = 0;
    private int mPositionHandle = 0;
    private int mTextureHandle = 0;
    private float mRotationAngle = 0.0f;

    public CompositionRenderer(int instances) {
        this.mInstances = instances;
        this.mCompositionVertices =
                ByteBuffer.allocateDirect(verticesData.length * 4).order(ByteOrder.nativeOrder())
                        .asFloatBuffer();
        this.mMVPMatrix = new float[16];
        this.mSTMatrix = new float[16];
        this.mTextureID = new int[this.mInstances];
        this.mCompositionVertices.put(verticesData).position(0);

        Matrix.setIdentityM(this.mSTMatrix, 0);
        mProgram = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
        if (mProgram == 0) {
            try {
                throw new Exception("failed creating program");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        this.mPositionHandle = GLES20.glGetAttribLocation(this.mProgram, "position");
        this.checkEglError("glGetAttribLocation aPosition");
        if (this.mPositionHandle == -1) {
            try {
                throw new Exception("Could not get attrib location for position");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        this.mTextureHandle = GLES20.glGetAttribLocation(this.mProgram, "textureCoord");
        this.checkEglError("glGetAttribLocation aTextureCoord");
        if (this.mTextureHandle == -1) {
            try {
                throw new Exception("Could not get attrib location for textureCoord");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        this.mMVPMatrixHandle = GLES20.glGetUniformLocation(this.mProgram, "mvpMatrix");
        this.checkEglError("glGetUniformLocation mvpMatrix");
        if (this.mMVPMatrixHandle == -1) {
            try {
                throw new Exception("Could not get attrib location for mvpMatrix");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        this.mSTMatrixHandle = GLES20.glGetUniformLocation(this.mProgram, "stMatrix");
        this.checkEglError("glGetUniformLocation stMatrix");
        if (this.mSTMatrixHandle == -1) {
            try {
                throw new Exception("Could not get attrib location for stMatrix");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        GLES20.glGenTextures(this.mInstances, this.mTextureID, 0);
        for (int index = 0; index < instances; index++) {
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, mTextureID[index]);
            checkEglError("glBindTexture textureID");
            GLES20.glTexParameterf(
                    GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER,
                    (float) GLES20.GL_NEAREST
            );
            GLES20.glTexParameterf(
                    GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER,
                    (float) GLES20.GL_LINEAR
            );
            GLES20.glTexParameteri(
                    GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S,
                    GLES20.GL_CLAMP_TO_EDGE
            );
            GLES20.glTexParameteri(
                    GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T,
                    GLES20.GL_CLAMP_TO_EDGE
            );
            checkEglError("glTexParameter");
        }
    }

    private int loadShader(int shaderType, String source) {
        int shader = GLES20.glCreateShader(shaderType);
        this.checkEglError("glCreateShader type=" + shaderType);
        GLES20.glShaderSource(shader, source);
        GLES20.glCompileShader(shader);
        int[] compiled = new int[1];
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compiled, 0);
        if (compiled[0] == 0) {
            Log.e(TAG, "Could not compile shader " + shaderType + ':');
            Log.e(TAG, " " + GLES20.glGetShaderInfoLog(shader));
            GLES20.glDeleteShader(shader);
            shader = 0;
        }
        return shader;
    }

    private int createProgram(String vertexSource, String fragmentSource) {
        int vertexShader = this.loadShader(GLES20.GL_VERTEX_SHADER, vertexSource);
        if (vertexShader == 0) {
            return 0;
        } else {
            int pixelShader = this.loadShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
            if (pixelShader == 0) {
                return 0;
            }
            int program = GLES20.glCreateProgram();
            this.checkEglError("glCreateProgram");
            if (program == 0) {
                Log.e(TAG, "Could not create program");
            }

            GLES20.glAttachShader(program, vertexShader);
            this.checkEglError("glAttachShader");
            GLES20.glAttachShader(program, pixelShader);
            this.checkEglError("glAttachShader");
            GLES20.glLinkProgram(program);
            int[] linkStatus = new int[1];
            GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
            if (linkStatus[0] != GLES20.GL_TRUE) {
                Log.e(TAG, "Could not link program: ");
                Log.e(TAG, GLES20.glGetProgramInfoLog(program));
                GLES20.glDeleteProgram(program);
                program = 0;
            }

            return program;

        }
    }

    public final int getTexID(int index) {
        return this.mTextureID[index];
    }

    public final void drawFrame(SurfaceTexture st, int width, int height) {
        this.checkEglError("DrawFrame");
        st.getTransformMatrix(this.mSTMatrix);
        this.checkEglError("getTransformMatrix");
        GLES20.glClearColor(0.0F, 1.0F, 0.0F, 0.0F);
        this.checkEglError("glClearColor");
        GLES20.glClear(GLES20.GL_DEPTH_BUFFER_BIT | GLES20.GL_COLOR_BUFFER_BIT);
        this.checkEglError("glClear");
        GLES20.glUseProgram(this.mProgram);
        this.checkEglError("glUseProgram");

        Matrix.setIdentityM(this.mMVPMatrix, 0);
        Matrix.rotateM(this.mMVPMatrix, 0, this.mRotationAngle, 0.0F, 0.0F, 1.0F);

        GLES20.glUniformMatrix4fv(this.mMVPMatrixHandle, 1, false, this.mMVPMatrix, 0);
        GLES20.glUniformMatrix4fv(this.mSTMatrixHandle, 1, false, this.mSTMatrix, 0);

        this.mCompositionVertices.position(VERTICES_DATA_POS_OFFSET);
        GLES20.glVertexAttribPointer(this.mPositionHandle, 3, GLES20.GL_FLOAT, false,
                VERTICES_DATA_STRIDE_BYTES, this.mCompositionVertices);
        this.checkEglError("glVertexAttribPointer position");

        GLES20.glEnableVertexAttribArray(this.mPositionHandle);
        this.checkEglError("glEnableVertexAttribArray positionHandle");

        this.mCompositionVertices.position(VERTICES_DATA_UV_OFFSET);
        GLES20.glVertexAttribPointer(this.mTextureHandle, 2, GLES20.GL_FLOAT, false,
                VERTICES_DATA_STRIDE_BYTES, this.mCompositionVertices);
        this.checkEglError("glVertexAttribPointer textureHandle");
        GLES20.glEnableVertexAttribArray(this.mTextureHandle);
        this.checkEglError("glEnableVertexAttribArray textureHandle");


        int gridSize = (int) Math.ceil(Math.sqrt(mInstances));
        int numberVertically = (int) Math.ceil((double) mInstances / gridSize);
        int extra = gridSize * numberVertically - mInstances;

        double baseHeight = (double)height / numberVertically;
        double baseWidth = (double) width / gridSize;

        int gridsOccupied = 0;
        for (int index = 0; index < mInstances; index++) {
            double hOffset = ((int)(gridsOccupied / gridSize)) * baseHeight;
            double wOffset = (gridsOccupied % gridSize) * baseWidth;

            if (extra > 0 && gridsOccupied % gridSize == gridSize - 2) {
                int rectWidthEnd = Math.min(width,(int)Math.ceil(wOffset + baseWidth * 2));
                int rectWidth = rectWidthEnd - (int)Math.floor(wOffset);

                int rectHeightEnd = Math.min(height,(int)Math.ceil(hOffset + baseHeight));
                int rectHeight = rectHeightEnd - (int)Math.floor(hOffset);

                GLES20.glViewport((int)Math.floor(wOffset), (int)Math.floor(hOffset), rectWidth, rectHeight);
                extra--;
                gridsOccupied += 2;
            } else {
                int rectWidthEnd = Math.min(width,(int)Math.ceil(wOffset + baseWidth));
                int rectWidth = rectWidthEnd - (int)Math.floor(wOffset);

                int rectHeightEnd = Math.min(height,(int)Math.ceil(hOffset + baseHeight));
                int rectHeight = rectHeightEnd - (int)Math.floor(hOffset);

                GLES20.glViewport((int)Math.floor(wOffset), (int)Math.floor(hOffset), rectWidth, rectHeight);
                gridsOccupied++;
            }

            GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, mTextureID[index]);
            GLES20.glUniform1i(GLES20.glGetUniformLocation(mProgram, "surfaceTexture"), 0);

            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            checkEglError("glDrawArrays");
        }
        GLES20.glFinish();
    }

    public final void setRotation(float rotation) {
        this.mRotationAngle = rotation;
    }

    private void checkEglError(String msg) {
        int error = EGL14.eglGetError();
        if (error != EGL14.EGL_SUCCESS) {
            try {
                throw new Exception(msg + ": EGL error: 0x" + Integer.toHexString(error));
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}


public final class VideoComposer {
    private final Object mCompositionSyncObject = new Object();
    private final Thread mCompositionThread;
    private final long mOutputFrameInterval;
    private final int mInstances;
    private final String TAG = "VideoComposer";
    private final ArrayList<InputComposerSurface> mInputSurface = new ArrayList<>();
    private final boolean mCompositionRunning = true;
    private EglCore mEglCore;
    private OutputComposerSurface mOutputSurface;
    private CompositionRenderer mCompositionRenderer;
    private long mOutputTimestamp = 0L;


    public VideoComposer(final Surface surface, int width, int height, float fps,
                         final float rotation, int instances) {
        this.mInstances = instances;
        this.mOutputFrameInterval = (long) (1.0E9F / fps);

        mCompositionThread = new CompositionThread(surface, width, height, rotation);
        mCompositionThread.start();

        synchronized (mCompositionSyncObject) {
            try {
                mCompositionSyncObject.wait();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    public final Surface getInputSurface(int index) {
        return this.mInputSurface.get(index).getSurface();
    }

    class CompositionThread extends Thread {
        Surface surface;
        int width;
        int height;
        float rotation;

        public CompositionThread(Surface surface, int width, int height, float rotation) {
            this.surface = surface;
            this.width = width;
            this.height = height;
            this.rotation = rotation;
        }

        //method where the thread execution will start
        public void run() {
            //logic to execute in a thread
            mEglCore = new EglCore();
            mOutputSurface = new OutputComposerSurface(mEglCore, surface);
            mOutputSurface.makeCurrent();
            mCompositionRenderer = new CompositionRenderer(mInstances);
            mCompositionRenderer.setRotation(rotation);
            for (int index = 0; index < mInstances; index++) {
                mInputSurface.add(
                        new InputComposerSurface(
                                mCompositionRenderer.getTexID(index),
                                width,
                                height
                        )
                );
            }

            synchronized (mCompositionSyncObject) {
                mCompositionSyncObject.notifyAll();
            }

            while (mCompositionRunning) {
                for (InputComposerSurface inputs : mInputSurface) {
                    if (!inputs.awaitFrame()) {
                        break;
                    }
                }
                mCompositionRenderer
                        .drawFrame(mInputSurface.get(0).getSurfaceTexture(), width, height);
                mOutputSurface.setPresentationTime(mOutputTimestamp);
                mOutputSurface.swapBuffers();
                mOutputTimestamp += mOutputFrameInterval;
            }
        }
    }
}



