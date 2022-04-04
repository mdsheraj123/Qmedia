/*
 *  Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted (subject to the limitations in the
 *  disclaimer below) provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *        contributors may be used to endorse or promote products derived
 *        from this software without specific prior written permission.
 *
 *  NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 *  GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 *  HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 *  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>

#include <string>
#include <mutex>
#include <condition_variable>

#include "ai_director_test.h"

const char *helpStr = "[-c cameraId]\n" \
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
  int camera_id = 0;

  while ((option = getopt (argc, argv, "c:")) != -1) {
    switch (option) {
      case 'c':
        camera_id = std::stoi(optarg);
        break;
      case '?':
      default:
        printf ("Usage: %s %s\n", argv[0], helpStr);
        return 0;
    }
  }

  UsecaseSetup usecase_setup;
  usecase_setup.process_width = 640;
  usecase_setup.process_height = 360;
  usecase_setup.transform_width = 1920;
  usecase_setup.transform_height = 1080;
  usecase_setup.output_width = 1920;
  usecase_setup.output_height = 1080;

  android::sp<AIDirectorTest> ai_test = new AIDirectorTest(usecase_setup, camera_id);

  if (int32_t res = ai_test->Initialize()) {
    printf ("AIDirectorTest initialization failed. res: %d\n", res);
    return -1;
  }

  ai_test->CameraStart();

  signal (SIGINT, handle_int_signal);

  std::unique_lock<std::mutex> lock(stop_mutex);
  stop_condidion.wait(lock, [] {return stop_flag;} );

  ai_test->CameraStop();

  return 0;
}
