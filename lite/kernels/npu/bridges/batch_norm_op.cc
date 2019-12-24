// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lite/kernels/npu/bridges/graph.h"
#include "lite/kernels/npu/bridges/registry.h"
#include "lite/kernels/npu/bridges/utility.h"

namespace paddle {
namespace lite {
namespace subgraph {
namespace npu {

int BatchNormConverter(void* ctx, OpLite* op, KernelBase* kernel) {
  CHECK(ctx != nullptr);
  CHECK(op != nullptr);
  auto graph = static_cast<Graph*>(ctx);
  auto op_info = op->op_info();
  auto op_type = op_info->Type();
  auto scope = op->scope();
  VLOG(3) << "[NPU] Converting " + op_type + "...";

  // Get input and output vars and op attributes
  auto x_name = op_info->Input("X").front();
  auto x_type = kernel->GetInputDeclType("X");
  CHECK(x_type->precision() == PRECISION(kFloat));
  CHECK(x_type->layout() == DATALAYOUT(kNCHW));
  auto x = scope->FindMutableTensor(x_name);
  auto x_dims = x->dims();
  auto scale_name = op_info->Input("Scale").front();
  auto scale_type = kernel->GetInputDeclType("Scale");
  CHECK(scale_type->precision() == PRECISION(kFloat));
  CHECK(scale_type->layout() == DATALAYOUT(kNCHW));
  auto scale = scope->FindMutableTensor(scale_name);
  auto bias_name = op_info->Input("Bias").front();
  auto bias_type = kernel->GetInputDeclType("Bias");
  CHECK(bias_type->precision() == PRECISION(kFloat));
  CHECK(bias_type->layout() == DATALAYOUT(kNCHW));
  auto bias = scope->FindMutableTensor(bias_name);
  auto mean_name = op_info->Input("Mean").front();
  auto mean_type = kernel->GetInputDeclType("Mean");
  CHECK(mean_type->precision() == PRECISION(kFloat));
  CHECK(mean_type->layout() == DATALAYOUT(kNCHW));
  auto mean = scope->FindMutableTensor(mean_name);
  auto variance_name = op_info->Input("Variance").front();
  auto variance_type = kernel->GetInputDeclType("Variance");
  CHECK(variance_type->precision() == PRECISION(kFloat));
  CHECK(variance_type->layout() == DATALAYOUT(kNCHW));
  auto variance = scope->FindMutableTensor(variance_name);
  auto y_name = op_info->Output("Y").front();
  auto y_type = kernel->GetOutputDeclType("Y");
  CHECK(y_type->precision() == PRECISION(kFloat));
  CHECK(y_type->layout() == DATALAYOUT(kNCHW));
  float momentum = op_info->GetAttr<float>("momentum");
  float epsilon = op_info->GetAttr<float>("epsilon");
  int mode = 1;  // bnScale, bnBias tensor dims are 1xCx1x1
  bool use_global_stats = op_info->GetAttr<bool>("use_global_stats");

  // X node
  std::shared_ptr<ge::Operator> x_node = nullptr;
  if (graph->HasNode(x_name)) {
    x_node = graph->GetNode(x_name);
  } else {
    x_node = graph->AddNode(x_name, x_dims);
  }

  // Scale, Bias, Mean, Variance node
  auto scale_const_node = graph->AddNode(scale_name, *scale);
  auto bias_const_node = graph->AddNode(bias_name, *bias);
  auto mean_const_node = graph->AddNode(mean_name, *mean);
  auto variance_const_node = graph->AddNode(variance_name, *variance);

  // Batch Norm node
  auto batch_norm_node = graph->AddNode<ge::op::BatchNormExt2>(y_name);
  batch_norm_node->set_input_x(*x_node);
  batch_norm_node->set_input_scale(*scale_const_node);
  batch_norm_node->set_input_offset(*bias_const_node);
  batch_norm_node->set_input_mean(*mean_const_node);
  batch_norm_node->set_input_variance(*variance_const_node);
  batch_norm_node->set_attr_momentum(momentum);
  batch_norm_node->set_attr_epsilon(epsilon);
  batch_norm_node->set_attr_mode(mode);
  batch_norm_node->set_attr_use_global_stats(use_global_stats);
  return SUCCESS;
}

}  // namespace npu
}  // namespace subgraph
}  // namespace lite
}  // namespace paddle

REGISTER_SUBGRAPH_BRIDGE(NPU,
                         batch_norm,
                         paddle::lite::subgraph::npu::BatchNormConverter);