/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc.
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
#ifndef MIOPEN_GEMM_HPP_
#define MIOPEN_GEMM_HPP_

#include <miopen/common.hpp>
#include <miopen/miopen.h>
#include <miopen/object.hpp>
#include <vector>

namespace miopen {

struct Handle;
struct TensorDescriptor;

struct GemmDesc : miopenGemmDescriptor
{
    GemmDesc();
    GemmDesc(int m_,
                      int n_,
                      int k_,
                      long long int lda_,
                      long long int ldb_,
                      long long int ldc_,
                      miopenDataType_t dataType_);

    GemmDesc(bool isColMajor_,
                      bool transA_,
                      bool transB_,
                      int m_,
                      int n_,
                      int k_,
                      int lda_,
                      int ldb_,
                      int ldc_,
                      long long int strideA_,
                      long long int strideB_,
                      long long int strideC_,
                      double alpha_,
                      double beta_,
                      int batch_count_,
                      miopenDataType_t dataType_);

    bool GetIsColMajor() const;
    bool GetTransA() const;
    bool GetTransB() const;
    int GetM() const;
    int GetN() const;
    int GetK() const;
    int GetldA() const;
    int GetldB() const;
    int GetldC() const;
    long long int GetStrideA() const;
    long long int GetStrideB() const;
    long long int GetStrideC() const;
    double GetAlpha() const;
    double GetBeta() const;
    int GetBatchCount() const;
    miopenDataType_t GetMIOpenDataType() const;

    void SetIsColMajor(bool);

    // stream out operator overloading for MIOpen log functions
    friend std::ostream& operator<<(std::ostream& stream, const GemmDesc& x);

    // private:
    bool isColMajor;
    bool transA, transB;
    int m, n, k;
    int ldA, ldB, ldC; // leading dimension 
    long long int strideA, strideB, strideC;
    double alpha, beta;
    int batch_count;
    miopenDataType_t dataType;
};

} // namespace miopen
MIOPEN_DEFINE_OBJECT(miopenGemmDescriptor, miopen::GemmDesc);
#endif // _MIOPEN_GEMM_HPP_
