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

#ifndef SNPE_WRAPPER_DUMMY_H_
#define SNPE_WRAPPER_DUMMY_H_

#include <vector>

#include "snpe_wrapper_interface.h"


class SNPEContext : public ISNPEContext {
 public:
  SNPEContext(NetworkIO io_type, TensorType output_tensor_type);
  ~SNPEContext();

  int32_t LoadModel(std::string& model_path) override;
  void ConfigureRuntime(uint32_t runtime) override;
  int32_t InitFramework() override;
  size_t GetNumberOfResultLayers() override;
  void SetInputBuffer(void *vaddr) override;
  int32_t ExecuteModel() override;
  void SetOutputBuffers(std::vector<SnpeTensorBuffer> buffers) override;
  TensorType GetInputTensorType() override;
  int32_t GetInputTensorRank() override;
  float GetInputMax() override;
  float GetInputMin() override;
  float GetInputStepSize() override;
  uint64_t GetInputStepExactly0() override;
  std::vector<size_t> GetInputDims() override;

  TensorType GetOutputTensorType(std::string name) override;
  int32_t GetOutputTensorRank(std::string name) override;
  float GetOutputMax(std::string name) override;
  float GetOutputMin(std::string name) override;
  float GetOutputStepSize(std::string name) override;
  uint64_t GetOutputStepExactly0(std::string name) override;
  std::vector<size_t> GetOutputDims(std::string name) override;
  size_t GetOutputSize(std::string name) override;

  void SetOutputLayers(std::vector<std::string> layers) override;
  std::vector<std::string> GetResultLayers() override;
  void SetBufferAddress(std::string name, void *vaddr) override;

 private:
};

#endif //SNPE_WRAPPER_DUMMY_H_