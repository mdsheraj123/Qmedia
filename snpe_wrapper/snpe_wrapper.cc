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

#include "snpe_wrapper.h"

#include <utils/Log.h>

SNPEContext::SNPEContext(NetworkIO io_type, TensorType output_tensor_type)
    : max_num_objects_(0),
      io_type_(io_type),
      input_tensor_width_(0),
      input_tensor_height_(0),
      output_tensor_type_(output_tensor_type) {
}

SNPEContext::~SNPEContext() {
  for (auto &item : snpe_params_.inbuf_map) {
    if (item.second.data) {
      delete[] item.second.data;
    }
  }

  for (auto &item : snpe_params_.outbuf_map) {
    if (item.second.data) {
      delete[] item.second.data;
    }
  }
}

void SNPEContext::PrintErrorStringAndExit() {
  const char* const err = zdl::DlSystem::getLastErrorString();
  ALOGE(" %s", err);
}

std::unique_ptr<zdl::DlContainer::IDlContainer> SNPEContext::LoadContainerFromFile(
    std::string container_path) {
  std::unique_ptr<zdl::DlContainer::IDlContainer> container;
  container = zdl::DlContainer::IDlContainer::open(zdl::DlSystem::String(container_path.c_str()));
  if (nullptr == container) {
    ALOGE("%s: Container loading failed", __func__);
    return nullptr;
  }

  return container;
}

int32_t SNPEContext::ExecuteModel() {
  ALOGI("%s: Enter", __func__);

  if (io_type_ == NetworkIO::kUserBuffer) {
    if (!snpe_params_.snpe->execute(snpe_params_.input_ub_map,
                                    snpe_params_.output_ub_map)) {
      PrintErrorStringAndExit();
      return -1;
    }
  } else if (io_type_ == NetworkIO::kITensor) {
    snpe_params_.output_tensor_map.clear();
    if (!snpe_params_.snpe->execute(snpe_params_.input_tensor_map,
                                    snpe_params_.output_tensor_map)) {
      PrintErrorStringAndExit();
      return -1;
    }
  } else {
    ALOGE("%s: Invalid Network IO value", __func__);
    return -1;
  }

  ALOGI("%s: Exit", __func__);
  return 0;
}

int32_t SNPEContext::LoadModel(std::string& model_path) {
  snpe_params_.container = LoadContainerFromFile(model_path);
  if (nullptr == snpe_params_.container) {
    PrintErrorStringAndExit();
    return -1;
  }
  return 0;
}

int32_t SNPEContext::CreateUserBuffer(BufferType type,
    const char * name) {
  zdl::DlSystem::IUserBufferFactory& ub_factory =
      zdl::SNPE::SNPEFactory::getUserBufferFactory();

  auto uba_opt = snpe_params_.snpe->getInputOutputBufferAttributes(name);
  if (!uba_opt) {
    throw std::runtime_error(
        std::string("Error obtaining attributes for tensor ") + name);
  }

  auto enc_type = (*uba_opt)->getEncodingType();
  ALOGI("Encoding type is %d", (int)enc_type);

  const zdl::DlSystem::TensorShape& buffer_shape = (*uba_opt)->getDims();

  std::unique_ptr<zdl::DlSystem::UserBufferEncoding> userBufferEncoding;
  zdl::DlSystem::UserBufferEncoding *m_encoding;
  size_t elem_size;

  if (type == BufferType::kOutput) {
    elem_size = ElementSizeOfTensor(output_tensor_type_);
    if (elem_size == sizeof(uint8_t))
    {
      userBufferEncoding = std::unique_ptr<zdl::DlSystem::UserBufferEncodingTf8>(
          new zdl::DlSystem::UserBufferEncodingTf8(0, 1.0));
    } else {
      userBufferEncoding = std::unique_ptr<zdl::DlSystem::UserBufferEncodingFloat>(
          new zdl::DlSystem::UserBufferEncodingFloat());
    }
    m_encoding = userBufferEncoding.get();
  } else {
    elem_size = (*uba_opt)->getElementSize();
    m_encoding = (*uba_opt)->getEncoding();
  }

  ALOGI("Bufer type %d elements size in bytes: %zu", (int)type, elem_size);

  size_t buf_size = CalculateSizeFromDims(buffer_shape.rank(),
                                          buffer_shape.getDimensions(),
                                          elem_size);

  auto *buf_map = &snpe_params_.inbuf_map;
  auto *ub_map = &snpe_params_.input_ub_map;
  if (type == BufferType::kOutput) {
    buf_map = &snpe_params_.outbuf_map;
    ub_map = &snpe_params_.output_ub_map;
  }

  uint8_t *buffer = NULL;
  auto &result_layers = snpe_params_.result_layers;
  if ((type == BufferType::kInput && name != snpe_params_.input_layer) ||
      (type == BufferType::kOutput &&
      std::find(result_layers.begin(), result_layers.end(), name) == std::end(result_layers))) {
    // Not an input or output layer. Needs buffer allocation.
    buffer = new uint8_t[buf_size];
  }

  buf_map->emplace(name, UserBufferInfo({buf_size, buffer}));

  snpe_params_.ub_list.push_back(ub_factory.createUserBuffer(
      buffer, buf_size,
      GetStrides(buffer_shape, elem_size), m_encoding));

  ub_map->add(name, snpe_params_.ub_list.back().get());

  return 0;
}

int32_t SNPEContext::CreateTensor(BufferType type, const char* name) {
  zdl::DlSystem::ITensorFactory& tensor_factory =
      zdl::SNPE::SNPEFactory::getTensorFactory();

  auto tensor_opt = snpe_params_.snpe->getInputOutputBufferAttributes(name);
  if (!tensor_opt) {
    throw std::runtime_error(
        std::string("Error obtaining attributes for tensor ") + name);
  }
  const zdl::DlSystem::TensorShape& tensor_shape = (*tensor_opt)->getDims();

  size_t elem_size = (*tensor_opt)->getElementSize();
  ALOGI("Bufer type %d elements size in bytes: %zu", (int)type, elem_size);

  size_t buf_size = CalculateSizeFromDims(tensor_shape.rank(),
                                          tensor_shape.getDimensions(),
                                          elem_size);

  auto *tensor_map = &snpe_params_.input_tensor_map;
  if (type == BufferType::kOutput) {
    tensor_map = &snpe_params_.output_tensor_map;
  }

  snpe_params_.tensor_list.push_back(tensor_factory.createTensor(tensor_shape));
  tensor_map->add(name, snpe_params_.tensor_list.back().get());

  return 0;
}

void SNPEContext::ConfigureRuntime(uint32_t runtime) {
  switch (runtime) {
    case kSnpeDsp: {
      if (zdl::SNPE::SNPEFactory::isRuntimeAvailable(
          zdl::DlSystem::Runtime_t::DSP)) {
        runtime_ = zdl::DlSystem::Runtime_t::DSP;
        ALOGI("DSP runtime selected");
      } else {
        runtime_ = zdl::DlSystem::Runtime_t::CPU;
        ALOGI("CPU runtime selected, but DSP was configured");
      }
      break;
    }
    case kSnpeGpu: {
      if (zdl::SNPE::SNPEFactory::isRuntimeAvailable(
          zdl::DlSystem::Runtime_t::GPU)) {
        runtime_ = zdl::DlSystem::Runtime_t::GPU;
        ALOGI("GPU runtime selected");
      } else {
        runtime_ = zdl::DlSystem::Runtime_t::CPU;
        ALOGI("CPU runtime selected, but GPU was configured");
      }
      break;
    }
    case kSnpeAip: {
      if (zdl::SNPE::SNPEFactory::isRuntimeAvailable(
          zdl::DlSystem::Runtime_t::AIP_FIXED8_TF)) {
        runtime_ = zdl::DlSystem::Runtime_t::AIP_FIXED8_TF;
        ALOGI("AIP runtime selected");
      } else {
        runtime_ = zdl::DlSystem::Runtime_t::CPU;
        ALOGI("CPU runtime selected, but AIP was configured");
      }
      break;
    }
    case kSnpeCpu: {
      runtime_ = zdl::DlSystem::Runtime_t::CPU;
      ALOGI("CPU runtime selected");
      break;
    }
  }
}

std::unique_ptr<zdl::SNPE::SNPE> SNPEContext::SetBuilderOptions() {
  ALOGI("%s: Enter", __func__);
  std::unique_ptr <zdl::SNPE::SNPE> snpe;
  zdl::SNPE::SNPEBuilder snpeBuilder(snpe_params_.container.get());
  zdl::DlSystem::StringList output_layers;

  for (size_t i = 0; i < output_layers_.size(); i++) {
    output_layers.append(output_layers_[i].c_str());
  }

  if (io_type_ == NetworkIO::kUserBuffer) {
    snpe =
        snpeBuilder.setOutputLayers(output_layers).setRuntimeProcessor(runtime_)
            .setUseUserSuppliedBuffers(true).setCPUFallbackMode(true).build();
  } else if (io_type_ == NetworkIO::kITensor) {
    snpe =
        snpeBuilder.setOutputLayers(output_layers).setRuntimeProcessor(runtime_)
            .setUseUserSuppliedBuffers(false).setCPUFallbackMode(true).build();
  } else {
    ALOGE("%s: Invalid Network IO value", __func__);
    throw std::runtime_error("Invalid Network IO value");
  }

  ALOGI("%s: Exit", __func__);
  return snpe;
}

void SNPEContext::ConfigureDimensions() {
  zdl::DlSystem::Optional <zdl::DlSystem::StringList> names_opt;
  names_opt = snpe_params_.snpe->getInputTensorNames();
  const zdl::DlSystem::StringList& names = *names_opt;
  const char * name = names.at(0);
  auto uba_opt = snpe_params_.snpe->getInputOutputBufferAttributes(name);
  const zdl::DlSystem::TensorShape& buffer_shape = (*uba_opt)->getDims();
  const zdl::DlSystem::Dimension* dims = buffer_shape.getDimensions();

  input_tensor_height_ = dims[1];
  input_tensor_width_ = dims[2];

  // Obtain input and result layer names
  snpe_params_.input_layer = static_cast<std::string>(names.at(0));

  zdl::DlSystem::Optional <zdl::DlSystem::StringList> out_names;
  out_names = snpe_params_.snpe->getOutputTensorNames();
  const zdl::DlSystem::StringList& result_layers = *out_names;

  for (uint32_t i = 0; i < result_layers.size(); i++) {
    snpe_params_.result_layers.push_back(result_layers.at(i));
  }
}

int32_t SNPEContext::PopulateMap(BufferType type) {
  int32_t result = 0;
  zdl::DlSystem::Optional <zdl::DlSystem::StringList> names_opt;

  switch (type) {
    case BufferType::kInput:
      names_opt = snpe_params_.snpe->getInputTensorNames();
      break;
    case BufferType::kOutput:
      names_opt = snpe_params_.snpe->getOutputTensorNames();
      break;
    default:
      ALOGE("Error obtaining tensor names");
      throw std::runtime_error("Error obtaining tensor names");
  }

  const zdl::DlSystem::StringList& names = *names_opt;
  for (const char *name : names) {
    if (io_type_ == NetworkIO::kUserBuffer) {
      result = CreateUserBuffer(type, name);
    } else if (io_type_ == NetworkIO::kITensor) {
      result = CreateTensor(type, name);
    } else {
      ALOGE("Invalid Network IO value %d", static_cast<int32_t>(io_type_));
      result = -1;
    }

    if (0 != result) {
      break;
    }
  }
  return result;
}

int32_t SNPEContext::InitFramework() {
  ALOGI("%s Enter", __func__);
  version_ = zdl::SNPE::SNPEFactory::getLibraryVersion();
  ALOGI("SNPE version: %s", version_.toString().c_str());

  snpe_params_.snpe = SetBuilderOptions();
  if (nullptr == snpe_params_.snpe) {
    PrintErrorStringAndExit();
    return -1;
  }

  ConfigureDimensions();
  if (0 != PopulateMap(BufferType::kInput)) {
    ALOGE("Failed to InitFramework");
    return -1;
  }

  if (0 != PopulateMap(BufferType::kOutput)) {
    ALOGE("Failed to InitFramework");
    return -1;
  }

  ALOGI("%s Exit", __func__);
  return 0;
}

zdl::DlSystem::TensorShape SNPEContext::GetStrides(
    zdl::DlSystem::TensorShape dims, const size_t& element_size) {

  size_t count = dims.rank();
  size_t strides[count];

  strides[dims.rank() - 1] = element_size;
  size_t stride = strides[dims.rank() - 1];

  for (size_t i = dims.rank() - 1; i > 0; i--) {
    if (dims[i] == 0) {
      stride *= max_num_objects_;
    } else {
      stride *= dims[i];
    }
    strides[i - 1] = stride;
  }

  return zdl::DlSystem::TensorShape(strides, count);
}

size_t SNPEContext::ElementSizeOfTensor(TensorType type) {
  switch (type) {
    case TensorType::kFloat:
      return sizeof(float);
    case TensorType::kInt32:
      return sizeof(int32_t);
    case TensorType::kQuant8:
    case TensorType::kUint8:
      return sizeof(uint8_t);
    default:
      ALOGE("Usupported tensor type: %d\n", type);
      break;
  }
  return 0;
}

size_t SNPEContext::CalculateSizeFromDims(const size_t rank,
                                       const zdl::DlSystem::Dimension* dims,
                                       const size_t& element_size) {
  if (0 == rank) {
    return 0;
  }
  size_t size = element_size;
  for (size_t i = 0; i < rank; i++) {
    if (0 == dims[i]) {
      size *= max_num_objects_;
    } else {
      size *= dims[i];
    }
  }
  return size;
}

TensorType SNPEContext::WrappedTensorType(
    zdl::DlSystem::UserBufferEncoding::ElementType_t type) {
  switch (type) {
    case zdl::DlSystem::UserBufferEncoding::ElementType_t::FLOAT:
      return TensorType::kFloat;
    case zdl::DlSystem::UserBufferEncoding::ElementType_t::UNSIGNED8BIT:
      return TensorType::kUint8;
    case zdl::DlSystem::UserBufferEncoding::ElementType_t::TF8:
      return TensorType::kQuant8;
    default:
      ALOGE("Usupported format: %d\n", static_cast<uint32_t>(type));
      break;
  }
  return TensorType::kUnknown;
}

// Public

void SNPEContext::SetInputBuffer(void *vaddr) {
  zdl::DlSystem::IUserBuffer *usrbuffer =
      snpe_params_.input_ub_map.getUserBuffer(snpe_params_.input_layer.c_str());
  usrbuffer->setBufferAddress(vaddr);
}

void SNPEContext::SetOutputBuffers(std::vector<SnpeTensorBuffer> buffers) {
  int32_t buffer_idx = 0;
  for (auto name : snpe_params_.result_layers) {
    if (buffer_idx == buffers.size()) {
      ALOGE("Output buffers are less than result_layers: (%d, %d)\n",
        buffers.size(), snpe_params_.result_layers.size());
      break;
    }
    zdl::DlSystem::IUserBuffer *usrbuffer =
      snpe_params_.output_ub_map.getUserBuffer(name.c_str());
    usrbuffer->setBufferAddress(buffers.at(buffer_idx).vaddr);
  }
}

TensorType SNPEContext::GetInputTensorType() {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      snpe_params_.input_layer.c_str());
  auto type = (*intput_opt)->getEncodingType();
  return WrappedTensorType(type);
}

size_t SNPEContext::GetNumberOfResultLayers() {
  return snpe_params_.result_layers.size();
}

int32_t SNPEContext::GetInputTensorRank() {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      snpe_params_.input_layer.c_str());
  const zdl::DlSystem::TensorShape& buffer_shape = (*intput_opt)->getDims();
  return buffer_shape.rank();
}

float SNPEContext::GetInputMax() {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      snpe_params_.input_layer.c_str());
  zdl::DlSystem::UserBufferEncodingTfN *penc =
      dynamic_cast<zdl::DlSystem::UserBufferEncodingTfN *>(
          (*intput_opt)->getEncoding());
  return penc->getMax();
}

float SNPEContext::GetInputMin() {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      snpe_params_.input_layer.c_str());
  zdl::DlSystem::UserBufferEncodingTfN *penc =
      dynamic_cast<zdl::DlSystem::UserBufferEncodingTfN *>(
          (*intput_opt)->getEncoding());
  return penc->getMin();
}

float SNPEContext::GetInputStepSize() {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      snpe_params_.input_layer.c_str());
  zdl::DlSystem::UserBufferEncodingTfN *penc =
      dynamic_cast<zdl::DlSystem::UserBufferEncodingTfN *>(
          (*intput_opt)->getEncoding());

  return penc->getQuantizedStepSize();
}

uint64_t SNPEContext::GetInputStepExactly0() {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      snpe_params_.input_layer.c_str());
  zdl::DlSystem::UserBufferEncodingTfN *penc =
      dynamic_cast<zdl::DlSystem::UserBufferEncodingTfN *>(
          (*intput_opt)->getEncoding());
  return penc->getStepExactly0();
}

std::vector<size_t> SNPEContext::GetInputDims() {
  std::vector<size_t> input_dims;
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      snpe_params_.input_layer.c_str());
  const zdl::DlSystem::TensorShape& buffer_shape = (*intput_opt)->getDims();
  const zdl::DlSystem::Dimension* dims = buffer_shape.getDimensions();
  size_t rank = buffer_shape.rank();
  for (size_t i = 0; i < rank; i++) {
    input_dims.push_back(dims[i]);
  }
  return input_dims;
}

TensorType SNPEContext::GetOutputTensorType(std::string name) {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      name.c_str());
  auto type = (*intput_opt)->getEncodingType();
  return WrappedTensorType(type);
}

int32_t SNPEContext::GetOutputTensorRank(std::string name) {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      name.c_str());
  const zdl::DlSystem::TensorShape& buffer_shape = (*intput_opt)->getDims();
  return buffer_shape.rank();
}

float SNPEContext::GetOutputMax(std::string name) {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      name.c_str());
  zdl::DlSystem::UserBufferEncodingTfN *penc =
      dynamic_cast<zdl::DlSystem::UserBufferEncodingTfN *>(
          (*intput_opt)->getEncoding());
  return penc->getMax();
}

float SNPEContext::GetOutputMin(std::string name) {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      name.c_str());
  zdl::DlSystem::UserBufferEncodingTfN *penc =
      dynamic_cast<zdl::DlSystem::UserBufferEncodingTfN *>(
          (*intput_opt)->getEncoding());
  return penc->getMin();
}

float SNPEContext::GetOutputStepSize(std::string name) {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      name.c_str());
  zdl::DlSystem::UserBufferEncodingTfN *penc =
      dynamic_cast<zdl::DlSystem::UserBufferEncodingTfN *>(
          (*intput_opt)->getEncoding());

  return penc->getQuantizedStepSize();
}

uint64_t SNPEContext::GetOutputStepExactly0(std::string name) {
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      name.c_str());
  zdl::DlSystem::UserBufferEncodingTfN *penc =
      dynamic_cast<zdl::DlSystem::UserBufferEncodingTfN *>(
          (*intput_opt)->getEncoding());
  return penc->getStepExactly0();
}

std::vector<size_t> SNPEContext::GetOutputDims(std::string name) {
  std::vector<size_t> output_dims;
  auto intput_opt = snpe_params_.snpe->getInputOutputBufferAttributes(
      name.c_str());
  const zdl::DlSystem::TensorShape& buffer_shape = (*intput_opt)->getDims();
  const zdl::DlSystem::Dimension* dims = buffer_shape.getDimensions();
  size_t rank = buffer_shape.rank();
  for (size_t i = 0; i < rank; i++) {
    output_dims.push_back(dims[i]);
  }
  return output_dims;
}

size_t SNPEContext::GetOutputSize(std::string name) {
  return snpe_params_.outbuf_map.at(name).size;
}

void SNPEContext::SetOutputLayers(std::vector<std::string> layers) {
  output_layers_ = layers;
}

std::vector<std::string> SNPEContext::GetResultLayers() {
  std::vector<std::string> result_layers;
  zdl::DlSystem::Optional <zdl::DlSystem::StringList> out_names;
  out_names = snpe_params_.snpe->getOutputTensorNames();
  const zdl::DlSystem::StringList& layers = *out_names;
  for (int32_t i = 0; i < layers.size(); i++) {
    result_layers.push_back(layers.at(i));
  }
  return result_layers;
}

void SNPEContext::SetBufferAddress(std::string name, void *vaddr) {
  zdl::DlSystem::IUserBuffer *usrbuffer =
      snpe_params_.output_ub_map.getUserBuffer(name.c_str());
  usrbuffer->setBufferAddress(vaddr);
}

extern "C" ISNPEContext* CreateSnpeContext(NetworkIO io_type, TensorType tensor_type) {
  return new SNPEContext(io_type, tensor_type);
}