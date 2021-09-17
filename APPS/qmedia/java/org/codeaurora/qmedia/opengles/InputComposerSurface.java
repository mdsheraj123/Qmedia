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
import android.view.Surface;

import java.util.concurrent.atomic.AtomicLong;

class FrameSync {
    private final Object mSync;
    private final long mSyncTimeout;
    private final boolean mIsReleased;
    private final AtomicLong mCount;
    private final int mSize;

    FrameSync(int size) {
        this.mSize = size;
        mIsReleased = false;
        this.mSync = new Object();
        this.mCount = new AtomicLong(0L);
        this.mSyncTimeout = 10L;
    }

    public final void notifyFrame() {
        if (!mIsReleased) {
            while (mCount.get() >= mSize) {
                synchronized (mSync) {
                    try {
                        mSync.wait(mSyncTimeout);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
            mCount.incrementAndGet();
            synchronized (mSync) {
                mSync.notifyAll();
            }
        }
    }

    public final long waitFrame() {
        long cnt = 0L;
        if (!mIsReleased) {
            while (mCount.get() <= 0) {
                synchronized (mSync) {
                    try {
                        mSync.wait(mSyncTimeout);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
            cnt = mCount.decrementAndGet();
            synchronized (mSync) {
                mSync.notifyAll();
            }
        }
        return mIsReleased ? -1L : cnt;
    }
}

public class InputComposerSurface implements SurfaceTexture.OnFrameAvailableListener {
    private final FrameSync mFrameSync = new FrameSync(10);
    private final String TAG = this.getClass().getSimpleName();
    private SurfaceTexture mSurfaceTexture;
    private Surface mSurface;


    public InputComposerSurface(int texName, int width, int height) {
        if (width > 0 && height > 0) {
            this.mSurfaceTexture = new SurfaceTexture(texName);
            this.mSurfaceTexture.setDefaultBufferSize(width, height);
            this.mSurfaceTexture
                    .setOnFrameAvailableListener((SurfaceTexture.OnFrameAvailableListener) this);
            this.mSurface = new Surface(this.mSurfaceTexture);
        } else {
            try {
                throw (Throwable) (new Exception("Invalid surface resolution"));
            } catch (Throwable throwable) {
                throwable.printStackTrace();
            }
        }
    }

    public final SurfaceTexture getSurfaceTexture() {
        return this.mSurfaceTexture;
    }

    public final Surface getSurface() {
        return this.mSurface;
    }

    public final boolean awaitFrame() {
        long count;

        do {
            count = this.mFrameSync.waitFrame();
            this.mSurfaceTexture.updateTexImage();
        } while (count > 0L);

        return count >= 0L;
    }


    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        mFrameSync.notifyFrame();
    }

}