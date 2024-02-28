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
#pragma once

#include "gru_common.hpp"

template <class T>
struct gru_driver : gru_basic_driver<T>
{
    gru_driver() : gru_basic_driver<T>()
    {
        std::vector<int> modes(2, 0);
        modes[1] = 1;
        std::vector<int> defaultBS(1);

        this->add(this->batchSize, "batch-size", this->generate_data(get_gru_batchSize(), {17}));
        this->add(this->seqLength, "seq-len", this->generate_data(get_gru_seq_len(), {2}));
        this->add(this->inVecLen, "vector-len", this->generate_data(get_gru_vector_len()));
        this->add(this->hiddenSize, "hidden-size", this->generate_data(get_gru_hidden_size()));
        this->add(this->numLayers, "num-layers", this->generate_data(get_gru_num_layers()));
        this->add(this->nohx, "no-hx", this->flag());
        this->add(this->nodhy, "no-dhy", this->flag());
        this->add(this->nohy, "no-hy", this->flag());
        this->add(this->nodhx, "no-dhx", this->flag());
        this->add(this->flatBatchFill, "flat-batch-fill", this->flag());
        this->add(this->useDropout, "use-dropout", this->generate_data({0}));

#if(MIO_GRU_TEST_DEBUG == 3)
        this->biasMode  = 0;
        this->dirMode   = 1;
        this->inputMode = 0;
#else
        this->add(this->inputMode, "in-mode", this->generate_data(modes));
        this->add(this->biasMode, "bias-mode", this->generate_data(modes));
        this->add(this->dirMode, "dir-mode", this->generate_data(modes));
#endif
        this->add(
            this->batchSeq,
            "batch-seq",
            this->lazy_generate_data(
                [=] { return generate_batchSeq(this->batchSize, this->seqLength); }, defaultBS));
    }
};
