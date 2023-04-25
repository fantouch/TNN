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

#include "tnn/device/cpu/acc/cpu_deconv_1d_layer_acc.h"

#include <algorithm>

#include "tnn/interpreter/layer_resource_generator.h"
#include "tnn/utils/dims_utils.h"
#include "tnn/utils/naive_compute.h"

namespace TNN_NS {
static int LeastCommonMultiple(int m, int n) {
    int a = m, b = n;
    while (a != b) {
        if (a > b) {
            a = a - b;
        } else {
            b = b - a;
        }
    }
    return m * n / a;
}

CpuDeconv1DLayerAcc::~CpuDeconv1DLayerAcc() {}

Status CpuDeconv1DLayerAcc::Init(Context *context, LayerParam *param, LayerResource *resource,
                               const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    CPU_CONVERT_HALF_RESOURCE(LAYER_DECONVOLUTION_1D);

    if (outputs[0]->GetBlobDesc().data_type == DATA_TYPE_INT8) {
        LOGE("CpuDeconvLayerAcc dont support DATA_TYPE_INT8");
        return Status(TNNERR_PARAM_ERR, "CpuDeconvLayerAcc dont support DATA_TYPE_INT8");
    }
    return TNN_OK;
}

Status CpuDeconv1DLayerAcc::Reshape(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    return TNN_OK;
}

Status CpuDeconv1DLayerAcc::Forward(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    if (inputs[0]->GetBlobDesc().data_type == DATA_TYPE_FLOAT) {
        return Exec<float>(inputs, outputs);
    } else if (inputs[0]->GetBlobDesc().data_type == DATA_TYPE_BFP16) {
        return Exec<bfp16_t>(inputs, outputs);
    }
    return Status(TNNERR_LAYER_ERR, "data type not support in deconv");
}

void CpuDeconv1DLayerAcc::ActiveOutput(ConvLayerParam *param, float &sum) {
    if (param->activation_type == ActivationType_ReLU) {
        sum = sum > 0.0f ? sum : 0.0f;
    } else if (param->activation_type == ActivationType_ReLU6) {
        if (sum > 6.0f) {
            sum = 6.0f;
        } else if (sum < 0.0f) {
            sum = 0.0f;
        }
    } else if(param->activation_type == ActivationType_SIGMOID_MUL) {
        sum = 1.0f / (1.0f + exp(-sum)) * sum;
    }
}

template <typename T>
Status CpuDeconv1DLayerAcc::Exec(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs) {
    auto param    = dynamic_cast<ConvLayerParam *>(param_);
    auto resource = dynamic_cast<ConvLayerResource *>(resource_);
    if (!param || !resource) {
        return Status(TNNERR_MODEL_ERR, "Error: DeconvLayerParam or DeconvLayerResource is empty");
    }

    Blob *input_blob  = inputs[0];
    Blob *output_blob = outputs[0];
    void *input_ptr   = input_blob->GetHandle().base;
    void *output_ptr  = output_blob->GetHandle().base;
    // NOTE: weight is format [n][i][o][w]
    // different form conv weight layout [n][o][i][w]
    void *weight_ptr   = resource->filter_handle.force_to<void *>();
    void *bias_ptr     = param->bias ? resource->bias_handle.force_to<void *>() : nullptr;
    DataType data_type = output_blob->GetBlobDesc().data_type;

    DimsVector output_dims = output_blob->GetBlobDesc().dims;
    DimsVector input_dims  = input_blob->GetBlobDesc().dims;
    const int batch        = output_dims[0];
    const int group        = param->group;

    const int output_channel           = output_dims[1];
    const int output_channel_per_group = output_channel / group;
    const int output_width             = output_dims[2];
    const int output_size              = output_width;

    const int input_channel           = input_dims[1];
    const int input_channel_per_group = input_channel / group;
    const int input_width             = input_dims[2];
    const int input_size              = input_width;

    const int pad_w_begin = param->pads[0];
    const int kernel_w    = param->kernels[0];
    const int kernel_size = kernel_w;

    const int stride_w = param->strides[0];
    const int dilation_w = param->dialations[0];

    const int delta_kx = LeastCommonMultiple(dilation_w, stride_w) / dilation_w;
    const int delta_ix = delta_kx * dilation_w / stride_w;

    if (data_type != DATA_TYPE_INT8) {
        // #pragma omp parallel
        for (int b = 0; b < batch; b++) {
            T *output_ptr_base = (T *)output_ptr + b * group * output_channel_per_group * output_size;
            T *input_ptr_base  = (T *)input_ptr + b * group * input_channel_per_group * input_size;
            for (int g = 0; g < group; g++) {
                const float *weight_ptr_g =
                    (float *)weight_ptr + g * input_channel_per_group * output_channel_per_group * kernel_size;
                const float *bias_g = bias_ptr ? (float *)bias_ptr + g * output_channel_per_group : nullptr;
                T *output_ptr_g     = output_ptr_base + g * output_channel_per_group * output_size;
                T *input_ptr_g      = input_ptr_base + g * input_channel_per_group * input_size;

                for (int oc = 0; oc < output_channel_per_group; oc++) {
                    const float bias      = bias_g ? bias_g[oc] : 0.f;
                    T *output_channel_ptr = output_ptr_g + oc * output_size;
                    for (int ow = 0; ow < output_width; ow++) {
                        T *outout_data_ptr = output_channel_ptr + ow;
                        float sum          = bias;

                        int ox     = ow + pad_w_begin;
                        int max_sx = std::min((input_width - 1) * stride_w, ox / stride_w * stride_w);
                        int min_kx = UP_DIV(ox - max_sx, dilation_w);
                        if ((ox - min_kx * dilation_w) % stride_w == 0) {
                            int min_sx = std::max(0, ROUND_UP(ox + dilation_w - kernel_w * dilation_w, stride_w));
                            int max_kx = (ox - min_sx) / dilation_w;
                            int min_ix = (ox - max_kx * dilation_w) / stride_w;

                            auto weight_data = weight_ptr_g + oc * kernel_size;
                            auto input_data  = (T *)input_ptr_g;
                            for (auto ic = 0; ic < input_channel_per_group; ic++) {
                                for (auto kx = max_kx, ix = min_ix; kx >= min_kx;
                                     kx -= delta_kx, ix += delta_ix) {
                                    auto wt4 = weight_data[ic * output_channel_per_group * kernel_size + kx];
                                    auto in4 = input_data[ic * input_size + ix];
                                    sum += float(in4) * wt4;
                                }
                            }
                        }
                        // post op : only support relu and relu6
                        ActiveOutput(param, sum);
                        *outout_data_ptr = sum;
                    }
                }
            }
        }
    } else {
        LOGE("Error: CpuDeconvLayerAcc layer acc dont support datatype: %d\n", data_type);
        return Status(TNNERR_MODEL_ERR, "Error: CpuDeconvLayerAcc layer acc dont support datatype");
    }
    return TNN_OK;
}

CpuTypeLayerAccRegister<TypeLayerAccCreator<CpuDeconv1DLayerAcc>> g_cpu_deconv_1d_layer_acc_register(LAYER_DECONVOLUTION_1D);

}  // namespace TNN_NS
