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

#include "snpe_wrapper_dummy.h"


SNPEContext::SNPEContext(NetworkIO io_type, TensorType output_tensor_type) {
}

SNPEContext::~SNPEContext() {
}

int32_t SNPEContext::ExecuteModel() {
  return 0;
}

int32_t SNPEContext::LoadModel(std::string& model_path) {
  return 0;
}

void SNPEContext::ConfigureRuntime(uint32_t runtime) {
}

int32_t SNPEContext::InitFramework() {
  return 0;
}

void SNPEContext::SetInputBuffer(void *vaddr) {
}

void SNPEContext::SetOutputBuffers(std::vector<SnpeTensorBuffer> buffers) {
}

TensorType SNPEContext::GetInputTensorType() {
  return TensorType::kUnknown;
}

size_t SNPEContext::GetNumberOfResultLayers() {
  return 0;
}

int32_t SNPEContext::GetInputTensorRank() {
  return 0;
}

float SNPEContext::GetInputMax() {
  return 0.0f;
}

float SNPEContext::GetInputMin() {
  return 0.0f;
}

float SNPEContext::GetInputStepSize() {
  return 0.0f;
}

uint64_t SNPEContext::GetInputStepExactly0() {
  return 0;
}

std::vector<size_t> SNPEContext::GetInputDims() {
  std::vector<size_t> input_dims;
  return input_dims;
}

TensorType SNPEContext::GetOutputTensorType(std::string name) {
  return TensorType::kUnknown;
}

int32_t SNPEContext::GetOutputTensorRank(std::string name) {
  return 0;
}

float SNPEContext::GetOutputMax(std::string name) {
  return 0.0f;
}

float SNPEContext::GetOutputMin(std::string name) {
  return 0.0f;
}

float SNPEContext::GetOutputStepSize(std::string name) {
  return 0.0f;
}

uint64_t SNPEContext::GetOutputStepExactly0(std::string name) {
  return 0;
}

std::vector<size_t> SNPEContext::GetOutputDims(std::string name) {
  std::vector<size_t> output_dims;
  return output_dims;
}

size_t SNPEContext::GetOutputSize(std::string name) {
  return 0;
}

void SNPEContext::SetOutputLayers(std::vector<std::string> layers) {
}

std::vector<std::string> SNPEContext::GetResultLayers() {
  std::vector<std::string> result_layers;
  return result_layers;
}

void SNPEContext::SetBufferAddress(std::string name, void *vaddr) {
}

extern "C" ISNPEContext* CreateSnpeContext(NetworkIO io_type, TensorType tensor_type) {
  return nullptr;
}