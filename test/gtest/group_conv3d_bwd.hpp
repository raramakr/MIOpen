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
#pragma once

#include "get_handle.hpp"
#include <miopen/conv/data_invoke_params.hpp>

#include "../driver/tensor_driver.hpp"
#include "conv_common.hpp"

template <typename T>
miopenDataType_t GetDataType();

template <>
miopenDataType_t GetDataType<float>()
{
    return miopenFloat;
}

template <>
miopenDataType_t GetDataType<half_float::half>()
{
    return miopenHalf;
}

template <>
miopenDataType_t GetDataType<int8_t>()
{
    return miopenInt8;
}

struct ConvTestCase
{
    size_t G;
    size_t N;
    size_t C;
    size_t D;
    size_t H;
    size_t W;
    size_t k;
    size_t z;
    size_t y;
    size_t x;
    size_t pad_x;
    size_t pad_y;
    size_t pad_z;
    size_t stride_x;
    size_t stride_y;
    size_t stride_z;
    size_t dilation_x;
    size_t dilation_y;
    size_t dilation_z;
    miopenConvolutionMode_t conv_mode;
    friend std::ostream& operator<<(std::ostream& os, const ConvTestCase& tc)
    {
        return os << " G:" << tc.G << " N:" << tc.N << " C:" << tc.C << " D:" << tc.D
                  << " H:" << tc.H << " W:" << tc.W << " k:" << tc.k << " z:" << tc.z
                  << " y:" << tc.y << " x:" << tc.x << " pad_z:" << tc.pad_z
                  << " pad_y:" << tc.pad_y << " pad_x:" << tc.pad_x << " stride_z:" << tc.stride_z
                  << " stride_y:" << tc.stride_y << " stride_x:" << tc.stride_x
                  << " dilation_z:" << tc.dilation_z << " dilation_y:" << tc.dilation_y
                  << " dilation_x:" << tc.dilation_x << " conv_mode:" << tc.conv_mode;
    }

    std::vector<size_t> GetInput() { return {N, C, D, H, W}; }
    std::vector<size_t> GetWeights() { return {k, C, z, y, x}; }

    miopen::ConvolutionDescriptor GetConv()
    {
        return miopen::ConvolutionDescriptor{
            3,
            miopenConvolution,
            miopenPaddingDefault,
            {static_cast<int>(pad_z), static_cast<int>(pad_y), static_cast<int>(pad_x)},
            {static_cast<int>(stride_z), static_cast<int>(stride_y), static_cast<int>(stride_x)},
            {static_cast<int>(dilation_z),
             static_cast<int>(dilation_y),
             static_cast<int>(dilation_x)},
            {0, 0, 0},
            static_cast<int>(G),
            1.0};
    }
};

std::vector<ConvTestCase> ConvTestConfigs()
{ // g    n   c   d    h   w   k   z  y  x pad_x pad_y pad_z stri_x stri_y stri_z dia_x dia_y dia_z
    return {{1, 128, 64, 14, 28, 28, 64, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, miopenConvolution},
            {1, 64, 32, 28, 28, 28, 32, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, miopenConvolution},
            {32, 128, 32, 28, 28, 28, 32, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, miopenConvolution},
            {16, 128, 16, 28, 28, 28, 16, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, miopenConvolution},
            {8, 128, 8, 28, 28, 28, 8, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, miopenConvolution},
            {4, 128, 4, 28, 28, 28, 4, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, miopenConvolution},
            {2, 128, 2, 28, 28, 28, 2, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, miopenConvolution}};
}

inline int SetTensorLayout(miopen::TensorDescriptor& desc)
{
    // get layout string names
    std::string layout_str = desc.GetLayout_str();

    std::vector<std::size_t> lens = desc.GetLengths();
    std::vector<int> int_lens(lens.begin(), lens.end());

    // set the strides for the tensor
    return SetTensorNd(&desc, int_lens, layout_str, desc.GetType());
}

template <typename T = float>
struct ConvBwdSolverTest
    : public ::testing::TestWithParam<
          std::tuple<miopenConvBwdDataAlgorithm_t, ConvTestCase, miopenTensorLayout_t>>
{
protected:
    void SetUp() override
    {
        test_skipped = false;

        std::tie(algo, conv_config, tensor_layout) = GetParam();
        input   = tensor<T>{miopen_type<T>{}, tensor_layout, conv_config.GetInput()};
        weights = tensor<T>{miopen_type<T>{}, tensor_layout, conv_config.GetWeights()};
        SetTensorLayout(input.desc);
        SetTensorLayout(weights.desc);
        auto gen_value = [](auto...) {
            return prng::gen_A_to_B(static_cast<T>(-3.0), static_cast<T>(3.0));
        };
        std::fill(input.begin(), input.end(), std::numeric_limits<double>::quiet_NaN());
        weights.generate(gen_value);
        conv_desc = conv_config.GetConv();

        miopen::TensorDescriptor output_desc =
            conv_desc.GetForwardOutputTensor(input.desc, weights.desc, GetDataType<T>());
        output = tensor<T>{miopen_type<T>{}, tensor_layout, output_desc.GetLengths()};
        SetTensorLayout(output.desc);
        output.generate(gen_value);

        auto&& handle = get_handle();
        in_dev        = handle.Write(input.data);
        wei_dev       = handle.Write(weights.data);
        out_dev       = handle.Write(output.data);
    }
    void TearDown() override
    {
        if(test_skipped)
            return;

        auto&& handle = get_handle();

        ref_in     = tensor<T>{miopen_type<T>{}, tensor_layout, conv_config.GetInput()};
        ref_in     = ref_conv_bwd(input, weights, output, conv_desc);
        input.data = handle.Read<T>(in_dev, input.data.size());
        EXPECT_FALSE(miopen::range_zero(ref_in)) << "Cpu data is all zeros";
        EXPECT_FALSE(miopen::range_zero(input)) << "Gpu data is all zeros";
        EXPECT_TRUE(miopen::range_distance(ref_in) == miopen::range_distance(input));

        const double tolerance = 80;
        double threshold       = std::numeric_limits<T>::epsilon() * tolerance;
        auto error             = miopen::rms_range(ref_in, input);

        EXPECT_FALSE(miopen::find_idx(ref_in, miopen::not_finite) >= 0)
            << "Non finite number found in the CPU data";

        EXPECT_TRUE(error < threshold)
            << "Error beyond tolerance Error:" << error << ",  Threshold: " << threshold;
    }
    ConvTestCase conv_config;
    miopen::ConvolutionDescriptor conv_desc;
    tensor<T> input;
    tensor<T> weights;
    tensor<T> output;
    tensor<T> ref_in;
    miopen::Allocator::ManageDataPtr in_dev;
    miopen::Allocator::ManageDataPtr wei_dev;
    miopen::Allocator::ManageDataPtr out_dev;
    miopenConvBwdDataAlgorithm_t algo = miopenConvolutionBwdDataAlgoImplicitGEMM;
    bool test_skipped                 = false;
    miopenTensorLayout_t tensor_layout;
};
