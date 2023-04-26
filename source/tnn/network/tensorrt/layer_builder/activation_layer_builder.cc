// Tencent is pleased to support the open source community by making TNN available.
//
// Copyright (C) 2020 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <vector>
#include <algorithm>

#include "tnn/network/tensorrt/layer_builder/activation_layer_builder.h"

namespace TNN_NS {

ActivationTRTLayerBuilder::ActivationTRTLayerBuilder(LayerType ignore) : TensorRTLayerBuilder(ignore) {
}

ILayer* ActivationTRTLayerBuilder::AddToNetwork(INetworkDefinition* network) {
    auto input_foreign_tensor = dynamic_cast<ForeignBlob*>(input_blobs_[0])->GetForeignTensor();
    auto output_foreign_tensor = dynamic_cast<ForeignBlob*>(output_blobs_[0])->GetForeignTensor();
    auto input_tensor = std::dynamic_pointer_cast<TensorRTTensor>(input_foreign_tensor)->GetTensor();
    bool type_cast = false;
    nvinfer1::DataType input_dtype = input_tensor->getType();
    if (input_dtype == nvinfer1::DataType::kINT32) {
        type_cast = true;
        ILayer* cast_layer = network->addIdentity(*input_tensor);
        if (cast_layer != nullptr) {
            cast_layer->setName((layer_name_+"_input_to_float").c_str());
            cast_layer->setOutputType(0, nvinfer1::DataType::kFLOAT);
            input_tensor = cast_layer->getOutput(0);
        }
    }


    ILayer* last_layer;
    IActivationLayer* activation_layer = network->addActivation(*input_tensor, m_type);
    if (activation_layer != nullptr) {
        activation_layer->setName(layer_name_.c_str());
        if (m_type == nvinfer1::ActivationType::kCLIP) {
            auto paramlist = dynamic_cast<ClipLayerParam *>(param_);
            activation_layer->setAlpha(std::max(-FLT_MAX, paramlist->min));
            activation_layer->setBeta(std::min(paramlist->max, FLT_MAX));
        }
        if (m_type == nvinfer1::ActivationType::kLEAKY_RELU) {
            auto resource = dynamic_cast<PReluLayerResource*>(resource_);
            auto scope = resource->slope_handle.force_to<float*>();
            activation_layer->setAlpha(*scope);
        }
        last_layer = activation_layer;
    }

    if (type_cast) {
        input_tensor = last_layer->getOutput(0);
        ILayer* cast_layer = network->addIdentity(*input_tensor);
        if (cast_layer != nullptr) {
            cast_layer->setName((layer_name_+"_output_to_int32").c_str());
            cast_layer->setOutputType(0, nvinfer1::DataType::kINT32);
            last_layer = cast_layer;
        }
    }

    return last_layer;
}

}  //  namespace TNN_NS
