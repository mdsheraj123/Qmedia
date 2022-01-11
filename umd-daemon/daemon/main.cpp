/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>

#include <string>
#include <mutex>
#include <condition_variable>

#include "umd-camera.h"

const char *helpStr = "[-v device] [-a device] [-m device] [-c cameraId]\n" \
                    "    -v : UVC video device\n" \
                    "    -a : UAC audio device\n" \
                    "    -m : Mic audio device\n" \
                    "    -c : Camera Id";

std::condition_variable stop_condidion;
std::mutex stop_mutex;
bool stop_flag = false;

void handle_int_signal (int signal)
{
  std::lock_guard<std::mutex> lock(stop_mutex);
  stop_flag = true;
  stop_condidion.notify_all();
}

int main(int argc, char * argv[]) {
  int32_t option = 0;
  std::string uvc_dev;
  std::string uac_dev;
  std::string mic_dev;
  int camera_id = 0;

  if (argc == 1) {
    printf ("Usage: %s %s\n", argv[0], helpStr);
    return 1;
  }

  while ((option = getopt (argc, argv, "v:a:c:m:")) != -1) {
    switch (option) {
      case 'v':
        uvc_dev = std::string(optarg);
        break;
      case 'a':
        uac_dev = std::string(optarg);
        break;
      case 'c':
        camera_id = std::stoi(optarg);
        break;
      case 'm':
        mic_dev = std::string(optarg);
        break;
      case '?':
      default:
        printf ("Usage: %s %s\n", argv[0], helpStr);
        return 0;
    }
  }

  android::sp<UmdCamera> umdcam = new UmdCamera(uvc_dev, uac_dev, mic_dev, camera_id);

  if (int32_t res = umdcam->Initialize()) {
    printf ("UmdCamera initialization failed. res: %d\n", res);
    return -1;
  }

  signal (SIGINT, handle_int_signal);

  std::unique_lock<std::mutex> lock(stop_mutex);
  stop_condidion.wait(lock, [] {return stop_flag;} );

  return 0;
}
