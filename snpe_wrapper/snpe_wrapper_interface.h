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

#ifndef SNPE_WRAPPER_INTF_H_
#define SNPE_WRAPPER_INTF_H_

#include <string>


typedef enum class NetworkIO {
  kUserBuffer = 0,
  kITensor
};

enum {
  kSnpeCpu,
  kSnpeDsp,
  kSnpeGpu,
  kSnpeAip
};

typedef enum class BufferType {
  kOutput = 0,
  kInput
};

typedef enum class TensorType {
  kFloat = 0,
  kInt32,
  kQuant8,
  kUint8,
  kUnknown
};

struct SnpeTensorBuffer {
  void *vaddr;
  size_t size;
};

class ISNPEContext {
 public:
  virtual ~ISNPEContext() {}
  virtual int32_t LoadModel(std::string& model_path) = 0;
  virtual void ConfigureRuntime(uint32_t runtime) = 0;
  virtual int32_t InitFramework() = 0;
  virtual size_t GetNumberOfResultLayers() = 0;
  virtual void SetInputBuffer(void *vaddr) = 0;
  virtual int32_t ExecuteModel() = 0;
  virtual void SetOutputBuffers(std::vector<SnpeTensorBuffer> buffers) = 0;
  virtual TensorType GetInputTensorType() = 0;
  virtual int32_t GetInputTensorRank() = 0;
  virtual float GetInputMax() = 0;
  virtual float GetInputMin() = 0;
  virtual float GetInputStepSize() = 0;
  virtual uint64_t GetInputStepExactly0() = 0;
  virtual std::vector<size_t> GetInputDims() = 0;

  virtual TensorType GetOutputTensorType(std::string name) = 0;
  virtual int32_t GetOutputTensorRank(std::string name) = 0;
  virtual float GetOutputMax(std::string name) = 0;
  virtual float GetOutputMin(std::string name) = 0;
  virtual float GetOutputStepSize(std::string name) = 0;
  virtual uint64_t GetOutputStepExactly0(std::string name) = 0;
  virtual std::vector<size_t> GetOutputDims(std::string name) = 0;
  virtual size_t GetOutputSize(std::string name) = 0;

  virtual void SetOutputLayers(std::vector<std::string> layers) = 0;
  virtual std::vector<std::string> GetResultLayers() = 0;
  virtual void SetBufferAddress(std::string name, void *vaddr) = 0;
};

extern "C" ISNPEContext* CreateSnpeContext(NetworkIO io_type, TensorType tensor_type);

#endif // SNPE_WRAPPER_INTF_H_