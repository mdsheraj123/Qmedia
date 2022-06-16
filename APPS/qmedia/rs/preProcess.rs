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

#pragma version(1)
#pragma rs_fp_relaxed
#pragma rs java_package_name(org.codeaurora.qmedia)

#include "rs_debug.rsh"

uint previewWidth, previewHeight, tensorWidth, tensorHeight;
uint picWidth, uvPixelStride, uvRowStride;
rs_allocation yIn, uIn, vIn, floatOutput;

uchar4 __attribute__((kernel)) doConvert(uint32_t x, uint32_t y) {

    uint32_t xFlip = previewWidth - 1 - x;
    uint32_t yFlip = previewHeight - 1 - y;

    uint uvIndex = uvPixelStride * (xFlip / 2) + uvRowStride * (yFlip / 2);

    uchar yp = rsGetElementAt_uchar(yIn, xFlip, yFlip);
    uchar u = rsGetElementAt_uchar(uIn, uvIndex);
    uchar v = rsGetElementAt_uchar(vIn, uvIndex);

    int4 argb;
    argb.r = yp + v * 1436 / 1024 - 179;
    argb.g = yp - u * 46549 / 131072 + 44 - v * 93604 / 131072 + 91;
    argb.b = yp + u * 1814 / 1024 - 227;
    argb.a = 255;

    uchar4 out = convert_uchar4(clamp(argb, 0, 255));

    int x_float = x * tensorWidth / previewWidth;
    int y_float = y * tensorHeight / previewHeight;

    rsSetElementAt_float(floatOutput, out.r / 255.0, (x_float + y_float * tensorWidth) * 3);
    rsSetElementAt_float(floatOutput, out.g / 255.0, (x_float + y_float * tensorWidth) * 3 + 1);
    rsSetElementAt_float(floatOutput, out.b / 255.0, (x_float + y_float * tensorWidth) * 3 + 2);

    return out;
}