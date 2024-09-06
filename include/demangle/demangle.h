#pragma once

#include <cxxabi.h>
#include <string>

namespace details {

    std::string _demangle(char const *name) {
        int status;
        char *realname = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        std::string result = realname ? realname : name;
        free(realname);
        return result;
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

}

using details::demangle;