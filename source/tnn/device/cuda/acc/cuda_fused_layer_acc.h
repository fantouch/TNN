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

#ifndef TNN_SOURCE_TNN_DEVICE_CUDA_ACC_CUDA_FUSED_LAYER_ACC_H_
#define TNN_SOURCE_TNN_DEVICE_CUDA_ACC_CUDA_FUSED_LAYER_ACC_H_

#include "tnn/device/cuda/acc/cuda_layer_acc.h"
#include "tnn/device/cuda/acc/compute/compute.h"

namespace TNN_NS {

class CudaFusedLayerAcc : public CudaLayerAcc {
public:
    virtual Status Init(Context *context, LayerParam *param, LayerResource *resource,
        const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs);
    virtual ~CudaFusedLayerAcc();
    virtual Status Reshape(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs);
    virtual Status Forward(const std::vector<Blob *> &inputs, const std::vector<Blob *> &outputs);

protected:
    FusedLayerParam *fused_param_;

    std::shared_ptr<cublasMMWrapper> cublas_fp32_;
    std::shared_ptr<cublasMMWrapper> cublas_fp16_;

    std::shared_ptr<FfnLayer<float>> ffn_layer_fp32_;
    std::shared_ptr<FfnLayer<half>> ffn_layer_fp16_;

    bool fp16_run_fused_attention_ = true;
    std::shared_ptr<BaseAttentionLayer<float>> attention_fp32_;
    std::shared_ptr<BaseAttentionLayer<half>> attention_fp16_;

    bool fp16_run_flash_attention_ = true;
    std::shared_ptr<FlashAttentionLayer<float>> flash_attention_fp32_;
    std::shared_ptr<FlashAttentionLayer<half>> flash_attention_fp16_;
    
    bool fp16_run_cross_attention_ = true;
    std::shared_ptr<CrossAttentionLayer<float>> cross_attention_fp32_;
    std::shared_ptr<CrossAttentionLayer<half>> cross_attention_fp16_;

    Status PrepareResource(RawBuffer &buf);
    Status PrepareFp32Resource(RawBuffer &buf);
    Status PrepareFp16Resource(RawBuffer &buf);

};

}  //  namespace TNN_NS

#endif  //  TNN_SOURCE_TNN_DEVICE_CUDA_ACC_CUDA_INNER_PRODUCT_LAYER_ACC_H_
