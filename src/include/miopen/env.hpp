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
#ifndef GUARD_MIOPEN_ENV_HPP
#define GUARD_MIOPEN_ENV_HPP

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <miopen/errors.hpp>

namespace miopen {

namespace internal {

template <typename T>
struct ParseEnvVal
{
};

template <>
struct ParseEnvVal<bool>
{
    static bool go(const char* vp)
    {
        std::string value_env_str{vp};

        for(auto& c : value_env_str)
        {
            if(std::isalpha(c) != 0)
            {
                c = std::tolower(static_cast<unsigned char>(c));
            }
        }

        if(value_env_str.compare("disable") == 0 || value_env_str.compare("disabled") == 0 ||
           value_env_str.compare("0") == 0 || value_env_str.compare("no") == 0 ||
           value_env_str.compare("off") == 0 || value_env_str.compare("false") == 0)
        {
            return false;
        }
        else if(value_env_str.compare("enable") == 0 || value_env_str.compare("enabled") == 0 ||
                value_env_str.compare("1") == 0 || value_env_str.compare("yes") == 0 ||
                value_env_str.compare("on") == 0 || value_env_str.compare("true") == 0)
        {
            return true;
        }
        else
        {
            MIOPEN_THROW(miopenStatusInvalidValue, "Invalid value for env variable");
        }

        return false; // shouldn't reach here
    }
};

template <>
struct ParseEnvVal<uint64_t>
{
    static uint64_t go(const char* vp) { return std::strtoull(vp, nullptr, 0); }
};

template <>
struct ParseEnvVal<std::string>
{
    static std::string go(const char* vp) { return std::string{vp}; }
};

template <typename T>
struct EnvVar
{
private:
    T value{};
    bool is_unset = true;

public:
    const T& GetValue() const { return value; }

    bool IsUnset() const { return is_unset; }

    void UpdateValue(const T& val)
    {
        is_unset = false;
        value    = val;
    }

    explicit EnvVar(const char* const name, const T& def_val)
    {
        // NOLINTNEXTLINE (concurrency-mt-unsafe)
        const char* vp = std::getenv(name);
        if(vp != nullptr) // a value was provided
        {
            is_unset = false;
            value    = ParseEnvVal<T>::go(vp);
        }
        else // no value provided, use default value
        {
            value = def_val;
        }
    }
};

} // end namespace internal

// static inside function hides the variable and provides
// thread-safety/locking
#define MIOPEN_DECLARE_ENV_VAR(name, type, default_val)                    \
    struct name                                                            \
    {                                                                      \
        using value_type = type;                                           \
        static miopen::internal::EnvVar<type>& Ref()                       \
        {                                                                  \
            static miopen::internal::EnvVar<type> var{#name, default_val}; \
            return var;                                                    \
        }                                                                  \
    };

#define MIOPEN_DECLARE_ENV_VAR_BOOL(name) MIOPEN_DECLARE_ENV_VAR(name, bool, false)

#define MIOPEN_DECLARE_ENV_VAR_UINT64(name) MIOPEN_DECLARE_ENV_VAR(name, uint64_t, 0)

#define MIOPEN_DECLARE_ENV_VAR_STR(name) MIOPEN_DECLARE_ENV_VAR(name, std::string, "")

/// \todo the following functions should be renamed to either include the word Env
/// or put inside a namespace 'env'. Right now we have a function named Value()
/// that returns env var value as only 64-bit ints

template <class EnvVar>
inline const std::string& GetStringEnv(EnvVar)
{
    static_assert(std::is_same_v<typename EnvVar::value_type, std::string>);
    return EnvVar::Ref().GetValue();
}

template <class EnvVar>
inline bool IsEnabled(EnvVar)
{
    static_assert(std::is_same_v<typename EnvVar::value_type, bool>);
    return !EnvVar::Ref().IsUnset() && EnvVar::Ref().GetValue();
}

template <class EnvVar>
inline bool IsDisabled(EnvVar)
{
    static_assert(std::is_same_v<typename EnvVar::value_type, bool>);
    return !EnvVar::Ref().IsUnset() && !EnvVar::Ref().GetValue();
}

template <class EnvVar>
inline uint64_t Value(EnvVar)
{
    static_assert(std::is_same_v<typename EnvVar::value_type, uint64_t>);
    return EnvVar::Ref().GetValue();
}

template <class EnvVar>
inline bool IsUnset(EnvVar)
{
    return EnvVar::Ref().IsUnset();
}

/// updates the cached value of an environment variable
template <typename EnvVar, typename ValueType>
void UpdateEnvVar(EnvVar, const ValueType& val)
{
    static_assert(std::is_same_v<typename EnvVar::value_type, ValueType>);
    EnvVar::Ref().UpdateValue(val);
}

} // namespace miopen

#endif
