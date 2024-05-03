/*******************************************************************************
 *
 * MIT License
 *
 * Copyright (c) 2024 Advanced Micro Devices, Inc.
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

#include <miopen/graphapi/tensor.hpp>
#include <miopen/graphapi/opgraph.hpp>

#include <functional>
#include <numeric>
#include <string_view>
#include <unordered_map>

namespace miopen {
namespace graphapi {

template <bool isVirtual, typename Vec>
Tensor makeTensor(std::string_view name, const Vec& dims, const Vec& strides) {
  int64_t id = 0;
  MIOPEN_THROW_IF(name.size() > sizeof(id), "tensor name exceeds 8 chars");
  std::copy_n(name.begin(), std::min(sizeof(id), name.size()), reinterpret_cast<char*>(&id));

  return TensorBuilder{}
    .setDataType(miopenFloat)
    .setDim(dims)
    .setStride(strides)
    .setId(id)
    .setVirtual(isVirtual)
    .build();
}

template <bool isVirtual, typename Vec>
Tensor makeTensor(std::string_view name, const Vec& dims) {

  Vec strides(dims);
  using T = typename Vec::value_type;
  std::exclusive_scan(dims.begin(), dims.end(), strides.begin(), T{1ll}, std::multiplies<T>{});

  return makeTensor<isVirtual>(name, dims, strides);
}

/// An RAII style class that captures a pointer to an object on heap and frees it
/// upon destruction. It's different from std::unique_ptr in that it allows
/// capturing multiple types of pointers
struct HeapPtrDeleter {
  using Fn = std::function<void()>;
  const Fn emptyFn = [](){};
  Fn mFn = emptyFn;

  template <typename T>
  explicit HeapPtrDeleter(T* ptr) {
    mFn = [ptr] () { delete ptr; };
  }

  HeapPtrDeleter(const HeapPtrDeleter&) = delete;
  HeapPtrDeleter& operator = (const HeapPtrDeleter&) = delete;

  HeapPtrDeleter(HeapPtrDeleter&& that) noexcept : mFn(std::move(that.mFn)) {
    that.mFn = emptyFn;
  }

  HeapPtrDeleter& operator = (HeapPtrDeleter&& that) noexcept { 
    this->mFn(); // destruct self.
    this->mFn = std::move(that.mFn);
    that.mFn = emptyFn;
    return *this;
  }

  ~HeapPtrDeleter() {
    mFn();
  }
};


/// an automatically deleting allocator that frees the allocated objects upon
/// destruction
struct AutoDeleteAllocator {
  std::vector<HeapPtrDeleter> mPtrsToFree;

  AutoDeleteAllocator() = default;
  AutoDeleteAllocator(const AutoDeleteAllocator&) = delete;
  AutoDeleteAllocator& operator = (const AutoDeleteAllocator&) = delete;

  AutoDeleteAllocator(AutoDeleteAllocator&&) = default;
  AutoDeleteAllocator& operator = (AutoDeleteAllocator&&) = default;
  ~AutoDeleteAllocator() = default;

  template <typename T>
  T* allocate(T&& val) {
    T* ret = new T(std::forward<T>(val));
    mPtrsToFree.emplace_back(ret);
    return ret;
  }
};

struct PatternGraphGenerator
{

    struct DummyNode : public OpNode
    {
        std::string mName;
        std::vector<Tensor*> mInTensors;
        std::vector<Tensor*> mOutTensors;

        DummyNode(const std::string& name,
                  const std::vector<Tensor*>& ins,
                  const std::vector<Tensor*>& outs)
            : mName(name), mInTensors(ins), mOutTensors(outs)
        {
        }

        const std::string& signName() const final { return mName; }

        std::vector<Tensor*> getInTensors() const final { return mInTensors; }

        std::vector<Tensor*> getOutTensors() const final { return mOutTensors; }
    };

    struct DummyNodeGenSpec
    {
        std::string mName;
        std::vector<std::string> mInTensors;
        std::vector<std::string> mOutTensors;
    };

    inline Tensor* makeDummyTensor(std::string_view name)
    {

        return mAlloc.allocate(makeTensor<true>(name, std::vector<int64_t>({1})));
    }

private:
    AutoDeleteAllocator mAlloc;
    OpGraph mGraph;

    PatternGraphGenerator(const std::vector<DummyNodeGenSpec>& node_specs): mGraph{}
    {

        std::unordered_map<std::string, Tensor*> tensor_map;
        std::vector<DummyNode> node_map;
        OpGraphBuilder builder;

        for(const auto& ns : node_specs)
        {
            std::vector<Tensor*> in_tensors;

            for(const auto& ti : ns.mInTensors)
            {
                auto [it, flag] = tensor_map.try_emplace(ti, makeDummyTensor(ti));
                in_tensors.emplace_back(it->second);
            }

            std::vector<Tensor*> out_tensors;
            for(const auto& to : ns.mOutTensors)
            {
                auto [it, flag] = tensor_map.try_emplace(to, makeDummyTensor(to));
                out_tensors.emplace_back(it->second);
            }

            builder.addNode(
                mAlloc.allocate(
                  DummyNode{ns.mName, in_tensors, out_tensors}));

        }

        mGraph = std::move(builder).build();
    }

public:
    PatternGraphGenerator() = default;
    PatternGraphGenerator(const PatternGraphGenerator&) = delete;
    PatternGraphGenerator& operator=(const PatternGraphGenerator&) = delete;
    PatternGraphGenerator(PatternGraphGenerator&&)      = default;
    PatternGraphGenerator& operator=(PatternGraphGenerator&&) = default;
    ~PatternGraphGenerator()                                  = default;

    static std::unique_ptr<PatternGraphGenerator>
    Make(const std::vector<DummyNodeGenSpec>& node_specs)
    {
        return std::unique_ptr<PatternGraphGenerator>(new PatternGraphGenerator(node_specs));
    }

    const auto& graph() const { return mGraph; }
};

} // end namespace graphapi
} // end namespace miopen
