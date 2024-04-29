/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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

#include <miopen/solver/conv_direct_naive_conv.hpp>
#include <miopen/solver.hpp>
#include <miopen/conv/wrw_invoke_params.hpp>
#include <miopen/env.hpp>

MIOPEN_DECLARE_ENV_VAR_BOOL(MIOPEN_DEBUG_CONV_DIRECT_NAIVE_CONV_WRW)

namespace miopen {
namespace solver {
namespace conv {

using ProblemDescription = miopen::conv::ProblemDescription;

bool ConvDirectNaiveConvWrw::IsApplicable(const ExecutionContext& ctx,
                                          const ProblemDescription& problem) const
{
    if(!miopen::debug::AlwaysEnableConvDirectNaive &&
       miopen::IsDisabled(ENV(MIOPEN_DEBUG_CONV_DIRECT_NAIVE_CONV_WRW)))
        return false;

    if(!ConvDirectNaiveConvIsApplicableByKernelType(ctx, problem))
        return false;

    if(!problem.IsLayoutDefault() && !problem.IsLayoutNHWC())
        return false;

    if(!(problem.IsFp32() || problem.IsFp16() || problem.IsBfp16() || problem.IsFp8() ||
         problem.IsBfp8()))
        return false;

    if(!problem.IsDirectionBackwardWrW())
        return false;
    if(!problem.AllTensorsLengthsFitIntoInt())
        return false;
    if(problem.IsTensorsCasted())
    {
        auto test_cast = [&](const TensorDescriptor& desc) {
            if(desc.GetCastType())
            {
                const auto cast_type = *desc.GetCastType();
                if(cast_type == miopenFloat8 || cast_type == miopenBFloat8)
                    return false;
            }
            // all tested tensors must have cast type set
            return true;
        };
        if(test_cast(problem.GetIn()))
            return false;
        if(test_cast(problem.GetOut()))
            return false;
    }

    return true;
}

ConvSolution ConvDirectNaiveConvWrw::GetSolution(const ExecutionContext& ctx,
                                                 const ProblemDescription& problem) const
{
    ConvSolution result;

    if(problem.Is2d())
    {
        if(IsAccInt32(problem))
        {
            result = conv_internal::GetConv2DWRWSolution<int32_t>(ctx, problem);
        }
        else
        {
            result = conv_internal::GetConv2DWRWSolution<double>(ctx, problem);
        }
    }
    else
    {
        if(IsAccInt32(problem))
        {
            result = conv_internal::GetConv3DWRWSolution<int32_t>(ctx, problem);
        }
        else
        {
            result = conv_internal::GetConv3DWRWSolution<double>(ctx, problem);
        }
    }
    return result;
}

} // namespace conv
} // namespace solver
} // namespace miopen
