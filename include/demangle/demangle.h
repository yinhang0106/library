#pragma once

#include <cxxabi.h>
#include <string>
#include <memory>

namespace demangle_details {

    std::string _demangle(char const *name) {
        int status;
        auto realname = std::unique_ptr<char>(abi::__cxa_demangle(name, nullptr, nullptr, &status));
        if (status != 0) {
            return name;
        }
        return realname.get();
    }

    template <typename T>
    std::string demangle() {
        std::string name{_demangle(typeid(std::remove_cvref_t<T>).name())};
        if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
            name += " const";
        }
        if constexpr (std::is_volatile_v<std::remove_reference_t<T>>) {
            name += " volatile";
        }
        if constexpr (std::is_lvalue_reference_v<T>) {
            name += " &";
        }
        if constexpr (std::is_rvalue_reference_v<T>) {
            name += " &&";
        }
        return name;
    }

}   // namespace demangle_details

using demangle_details::demangle;