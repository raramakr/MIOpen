/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2023 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *******************************************************************************/

#include <miopen/normalization/solvers.hpp>

#include <miopen/normalization/invoke_params.hpp>
#include <miopen/layernorm.hpp>
#include <miopen/kernel_build_params.hpp>

#define LOCAL_SIZE 256

namespace miopen {

namespace solver {

namespace normalization {

bool LayernormForward::IsApplicable(
    const ExecutionContext&, const miopen::normalization::ProblemDescription& problem) const
{
    if(problem.GetXDesc().GetType() != problem.GetYDesc().GetType())
    {
        MIOPEN_THROW(miopenStatusBadParm, "LayerNormForward: Tensor types do not match.");
    }

    if(problem.GetXDesc().GetLengths() != problem.GetYDesc().GetLengths())
    {
        MIOPEN_THROW(miopenStatusBadParm,
                     "LayerNormForward: Tensor dimension lengths do not match.");
    }

    bool is_all_packed = problem.GetXDesc().IsPacked() && problem.GetWeightDesc().IsPacked() &&
                         problem.GetBiasDesc().IsPacked() && problem.GetYDesc().IsPacked() &&
                         problem.GetMeanDesc().IsPacked() && problem.GetRstdDesc().IsPacked();

    if(!is_all_packed)
    {
        MIOPEN_THROW(miopenStatusBadParm, "LayerNormForward: Unpacked tensors not supported.");
    }
    return true;
}

ConvSolution
LayernormForward::GetSolution(const ExecutionContext& context,
                                  const miopen::normalization::ProblemDescription& problem) const
{
    const auto& handle = context.GetStream();

    auto result = ConvSolution{miopenStatusSuccess};

    {
        auto dtype = problem.GetXDesc().GetType();
        auto dims = problem.GetXDesc().GetLengths();

        size_t outer_size = 1;
        for(size_t i = 0 ; i < problem.GetNormalizedDim(); i++)
        {
            outer_size *= dims[i];
        }

        size_t xlocalsize = LOCAL_SIZE;
        size_t xgridsize  = outer_size * xlocalsize;
        size_t ylocalsize = 1;
        size_t ygridsize  = 1;
        size_t zlocalsize = 1;
        size_t zgridsize  = 1;

        auto kernel = KernelInfo{};

        kernel.kernel_file = "MIOpenLayerNorm.cpp";
        kernel.kernel_name = "LayernormFwdContiguous";

        const auto build_params = KernelBuildParameters{
            {"MIOPEN_USE_FP16", static_cast<int>(dtype == miopenHalf)},
            {"MIOPEN_USE_FP32", static_cast<int>(dtype == miopenFloat)},
            {"MIOPEN_USE_FP64", static_cast<int>(dtype == miopenDouble)},
            {"MIOPEN_USE_BFP16", static_cast<int>(dtype == miopenBFloat16)},
            {"LOCAL_SIZE", LOCAL_SIZE},
        };

        kernel.comp_options = build_params.GenerateFor(kbp::HIP{});

        kernel.l_wk.push_back(xlocalsize);
        kernel.l_wk.push_back(ylocalsize);
        kernel.l_wk.push_back(zlocalsize);

        kernel.g_wk.push_back(xgridsize);
        kernel.g_wk.push_back(ygridsize);
        kernel.g_wk.push_back(zgridsize);

        result.construction_params.push_back(kernel);
    }

    result.invoker_factory = [](const std::vector<Kernel>& kernels) {
        return [=](const Handle& handle_, const AnyInvokeParams& raw_params) {
            decltype(auto) kernel = handle_.Run(kernels.front());
            decltype(auto) params = raw_params.CastTo<miopen::normalization::InvokeParams>();

            auto dims          = params.xDesc->GetLengths();
            size_t inner_size_ = 1;

            for(size_t i = params.normalized_dim ; i < dims.size(); i++)
            {
                inner_size_ *= dims[i];
            }

            kernel(params.x,
                   params.y,
                   params.weight,
                   params.bias,
                   params.mean,
                   params.rstd,
                   params.epsilon,
                   inner_size_,
                   params.mode);
        };
    };

    return result;
}

} // namespace normalization

} // namespace solver

} // namespace miopen
