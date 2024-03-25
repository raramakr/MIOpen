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
#ifndef MIOPEN_DONT_USE_HIP_RUNTIME_HEADERS
#include <hip/hip_runtime.h>
#endif

#include "float_types.h"

template <typename T>
inline __device__ void AdamInternal(size_t gid,
                                    T* params,
                                    T grad,
                                    T* exp_avgs,
                                    T* exp_avg_sqs,
                                    T* max_exp_avg_sqs,
                                    double lr,
                                    double beta1,
                                    double beta2,
                                    double weight_decay,
                                    double eps,
                                    int step,
                                    bool amsgrad,
                                    bool maximize)
{
    T param      = params[gid];
    T exp_avg    = exp_avgs[gid];
    T exp_avg_sq = exp_avg_sqs[gid];

    double bias_correction1 = 1 - pow(beta1, step);
    double bias_correction2 = 1 - pow(beta2, step);

    if(maximize)
        grad *= -1;
    if(weight_decay != 0)
        grad += param * weight_decay;

    exp_avg    = exp_avg * beta1 + grad * (1 - beta1);
    exp_avg_sq = exp_avg_sq * beta2 + grad * grad * (1 - beta2);

    double denom;
    if(amsgrad)
    {
        T max_exp_avg_sq = max_exp_avg_sqs[gid];
        if(exp_avg_sq > max_exp_avg_sq)
        {
            max_exp_avg_sq       = exp_avg_sq;
            max_exp_avg_sqs[gid] = max_exp_avg_sq;
        }

        denom = sqrt(max_exp_avg_sq) / sqrt(bias_correction2) + eps;
    }
    else
    {
        denom = sqrt(exp_avg_sq) / sqrt(bias_correction2) + eps;
    }

    param -= (lr / bias_correction1) * exp_avg / denom;

    params[gid]      = param;
    exp_avgs[gid]    = exp_avg;
    exp_avg_sqs[gid] = exp_avg_sq;
}

extern "C" __global__ void AdamPacked(FLOAT* params,
                                      FLOAT* grads,
                                      FLOAT* exp_avgs,
                                      FLOAT* exp_avg_sqs,
                                      FLOAT* max_exp_avg_sqs,
                                      int* step,
                                      double lr,
                                      double beta1,
                                      double beta2,
                                      double weight_decay,
                                      double eps,
                                      bool amsgrad,
                                      bool maximize,
                                      long input_size)
{
    size_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    size_t lid = threadIdx.x;

    if(gid >= input_size)
        return;

    __shared__ int step_val;

    if(lid == 0)
        step_val = step[0];

    __syncthreads();

    AdamInternal<FLOAT>(gid,
                        params,
                        grads[gid],
                        exp_avgs,
                        exp_avg_sqs,
                        max_exp_avg_sqs,
                        lr,
                        beta1,
                        beta2,
                        weight_decay,
                        eps,
                        step_val,
                        amsgrad,
                        maximize);
}

extern "C" __global__ void AmpAdamPacked(FLOAT* params,
                                         FLOAT* grads,
                                         FLOAT* exp_avgs,
                                         FLOAT* exp_avg_sqs,
                                         FLOAT* max_exp_avg_sqs,
                                         int32_t* grad_scale,
                                         bool* inf_found,
                                         int* step,
                                         double lr,
                                         double beta1,
                                         double beta2,
                                         double weight_decay,
                                         double eps,
                                         bool amsgrad,
                                         bool maximize,
                                         long input_size)
{
    size_t gid = blockIdx.x * blockDim.x + threadIdx.x;
    size_t lid = threadIdx.x;

    if(gid >= input_size)
        return;

    __shared__ int step_val;
    __shared__ bool found_inf;
    __shared__ float scale_factor;

    if(lid == 0)
    {
        found_inf = (inf_found) ? *inf_found : false;
        if(!found_inf)
        {
            scale_factor = (grad_scale) ? *grad_scale : 1.0f;
            step_val     = *step + 1;
        }
    }
    __syncthreads();

    if(found_inf)
        return;

    FLOAT grad = grads[gid] / scale_factor;

    AdamInternal<FLOAT>(gid,
                        params,
                        grad,
                        exp_avgs,
                        exp_avg_sqs,
                        max_exp_avg_sqs,
                        lr,
                        beta1,
                        beta2,
                        weight_decay,
                        eps,
                        step_val,
                        amsgrad,
                        maximize);
}

extern "C" __global__ void AmpAdamUpdateStep(bool* found_inf, int* step)
{
    if(!*found_inf)
        *step += 1;
}