#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>
#include <optional>
#include <variant>


namespace printer_details {

template <typename T, typename = void>
struct is_tuple : std::false_type {};

template <typename T>
struct is_tuple<T, std::void_t<decltype(std::tuple_size<T>::value)>> : std::true_type {};

template <typename T>
struct is_array : std::false_type {};

template <typename T>
struct is_array<T[]> : std::true_type {};

template <typename T, std::size_t N>
struct is_array<std::array<T, N>> : std::true_type {};

template <typename T, std::size_t N>
struct is_array<T[N]> : std::true_type {};

template <typename T, typename = void>
struct is_map : std::false_type {};

template <typename T>
struct is_map<T, std::enable_if_t<std::is_same_v<typename T::value_type, std::pair<typename T::key_type const, typename T::mapped_type>>>> : std::true_type {};

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template <typename T>
struct is_variant : std::false_type {};

template <typename... Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {};

template <typename T, typename... Ts>
struct is_one_of : std::disjunction<std::is_same<T, Ts>...> {};

template <typename T>
#if __cplusplus >= 202002L
struct is_char : is_one_of<T, char, wchar_t, char8_t, char16_t, char32_t> {};
#else
struct is_char : is_one_of<T, char, wchar_t, char16_t, char32_t> {};
#endif

template <typename T, typename = void>
struct is_string : std::false_type {};

template <typename T>
struct is_string<std::basic_string<T>, std::enable_if_t<is_char<T>::value>> : std::true_type {};

template <typename T>
struct is_string<std::basic_string_view<T>, std::enable_if_t<is_char<T>::value>> : std::true_type {};

template <typename T>
struct is_c_str : std::conjunction<std::is_pointer<std::decay_t<T>>, is_char<std::remove_const_t<std::remove_pointer_t<std::decay_t<T>>>>> {};

template <typename T, typename = void>
struct is_iterable : std::false_type {};

template <typename T>
struct is_iterable<T, std::void_t<decltype(std::begin(std::declval<T const &>()))>> : std::true_type {};

template <typename T>
constexpr bool is_map_v = is_map<T>::value;

template <typename T>
constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
constexpr bool is_variant_v = is_variant<T>::value;

template <typename T>
constexpr bool is_char_v = is_char<T>::value;

template <typename T>
constexpr bool is_string_v = is_string<T>::value;

template <typename T>
constexpr bool is_c_str_v = is_c_str<T>::value;

template <typename T>
constexpr bool is_iterable_v = is_iterable<T>::value;

template <typename T>
constexpr bool is_tuple_v = is_tuple<T>::value;

template <typename T>
constexpr bool is_array_v = is_array<T>::value;

template <typename T, typename = void>
struct _printer {
    static void print(std::ostream& os, T const& val) {
        os << val;
    }
};

template <class T>
struct _printer<T, std::enable_if_t<is_char_v<T>>> {
static void print(std::ostream &os, T const &t) {
    T s[2] = {t, T('\0')};
    os << std::quoted(s, T('\''));
}
};

template <typename T>
struct _printer<T, std::enable_if_t<is_tuple_v<T> && !is_array_v<T>>> {
static void print(std::ostream& os, T const& val) {
    os << "(";
    bool once = false;
    std::apply([&os, &once](auto const&... args) {
        ((once ? os << ", " : os, once = true, _printer<std::remove_cvref_t<decltype(args)>>::print(os, args)), ...);
    }, val);
    os << ")";
}
};

template <typename T>
struct _printer<T, std::enable_if_t<is_iterable_v<T> && !is_map_v<T> && !is_c_str_v<T> && !is_string_v<T>>> {
static void print(std::ostream& os, T const& val) {
    os << "[";
    bool once = false;
    for (auto const& v : val) {
        if (once) os << ", ";
        once = true;
        _printer<std::remove_cvref_t<decltype(v)>>::print(os, v);
    }
    os << "]";
}
};

template <typename T>
struct _printer<T, std::enable_if_t<is_map_v<T>>> {
static void print(std::ostream& os, T const& val) {
    os << "{";
    bool once = false;
    for (auto const& [k, v] : val) {
        if (once) os << ", ";
        once = true;
        _printer<typename T::key_type>::print(os, k);
        os << ": ";
        _printer<typename T::mapped_type>::print(os, v);
    }
    os << "}";
}
};

template <typename T>
struct _printer<T, std::enable_if_t<is_optional_v<T>>> {
static void print(std::ostream& os, T const& val) {
    if (val.has_value()) {
        _printer<typename T::value_type>::print(os, val.value());
    } else {
        os << "nullopt";
    }
}
};

template <typename T>
struct _printer<T, std::enable_if_t<is_variant_v<T>>> {
static void print(std::ostream& os, T const& val) {
    std::visit([&os](auto const& v) {
        _printer<std::remove_cvref_t<decltype(v)>>::print(os, v);
    }, val);
}
};

template <>
struct _printer<std::nullptr_t> {
    static void print(std::ostream &os, std::nullptr_t const &) {
        os << "nullptr";
    }
};

template <>
struct _printer<std::nullopt_t> {
    static void print(std::ostream& os, std::nullopt_t const&) {
        os << "nullopt";
    }
};

template <>
struct _printer<std::monostate> {
    static void print(std::ostream& os, std::monostate const&) {
        os << "monostate";
    }
};

template <typename T0, typename ...Ts>
void print(T0 const &t0, Ts const &...ts) {
    [] (std::ostream &os, T0 const& t0, Ts const&... ts) {
        _printer<std::remove_cvref_t<T0>>::print(os, t0);
        ((os << " ", _printer<std::remove_cvref_t<Ts>>::print(os, ts)), ...);
        os << "\n";
    } (std::cout, t0, ts...);
}

template <typename T0, typename ...Ts>
void printnl(T0 const &t0, Ts const&... ts) {
    [] (std::ostream &os, T0 const& t0, Ts const&... ts) {
        _printer<std::remove_cvref_t<T0>>::print(os, t0);
        ((os << " ", _printer<std::remove_cvref_t<Ts>>::print(os, ts)), ...);
    } (std::cout, t0, ts...);
}

template <typename T0, typename ...Ts>
void eprint(T0 const &t0, Ts const &...ts) {
    [] (std::ostream &os, T0 const& t0, Ts const&... ts) {
        _printer<std::remove_cvref_t<T0>>::print(os, t0);
        ((os << " ", _printer<std::remove_cvref_t<Ts>>::print(os, ts)), ...);
        os << "\n";
    } (std::cerr, t0, ts...);
}

template <typename T0, typename ...Ts>
void eprintnl(T0 const &t0, Ts const&... ts) {
    [] (std::ostream &os, T0 const& t0, Ts const&... ts) {
        _printer<std::remove_cvref_t<T0>>::print(os, t0);
        ((os << " ", _printer<std::remove_cvref_t<Ts>>::print(os, ts)), ...);
    } (std::cerr, t0, ts...);
}

template <typename T0, typename ...Ts>
std::string to_string(T0 const &t0, Ts const&... ts) {
    std::ostringstream oss;
    [] (std::ostringstream &oss, T0 const& t0, Ts const&... ts) {
        _printer<std::remove_cvref_t<T0>>::print(oss, t0);
        ((oss << " ", _printer<std::remove_cvref_t<Ts>>::print(oss, ts)), ...);
    } (oss, t0, ts...);
    return oss.str();
}

}   // namespace printer_details

using printer_details::print;
using printer_details::printnl;
using printer_details::eprint;
using printer_details::eprintnl;
using printer_details::to_string;
