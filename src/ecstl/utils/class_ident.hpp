#pragma once
#include <source_location>
#include "../polyfill/hasher.hpp"

namespace ecstl {

template<typename T>
struct ClassIdent {
    static constexpr std::string_view get_ident_string() {
        return std::source_location::current().function_name();
    }
    static constexpr std::size_t get_hash() {
        hash<std::string_view> hasher;
        return hasher(get_ident_string());
    }
};

template<typename T>
constexpr auto class_hash = ClassIdent<T>::get_hash();
template<typename T>
constexpr auto class_ident_string = ClassIdent<T>::get_ident_string();


}