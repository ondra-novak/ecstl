#pragma once
#include <string_view>
#include <source_location>
#include "../polyfill/hasher.hpp"
namespace ecstl {


template<typename __TypeNameArgument__>
struct GenerateTypeName {
    static constexpr std::string_view get_type_name() {
        auto cur = std::source_location::current();
        std::string_view name =  cur.function_name();
        auto pos = name.rfind("__TypeNameArgument__");
        if (pos == name.npos) {
            //msvc
            pos = name.find("GenerateTypeName<");
            if (pos == name.npos) {
                return name;
            }
            auto sn1 = name.substr(pos+17);
            pos = sn1.find(">::get_type_name");
            auto sn2 = sn1.substr(0,pos);
            if (sn1.substr(0,7) == "struct ") sn2 = sn2.substr(7);
            else if (sn1.substr(0,6) == "class ") sn2 = sn2.substr(6);
            else if (sn1.substr(0,5) == "enum ") sn2 = sn2.substr(5);
            return sn2;
            
        } else {
            auto sn1 = name.substr(pos);
            pos = sn1.find(" = ");
            auto sn2 = sn1.substr(pos+3);
            pos = sn2.find_first_of("];");
            return sn2.substr(0,pos);
        }
    }

    static constexpr std::size_t get_hash() {
        hash<std::string_view> hasher;
        return hasher(get_type_name());
    }

    
};

template<typename T>
static constexpr auto type_name = GenerateTypeName<T>::get_type_name();
template<typename T>
static constexpr auto type_name_hash = GenerateTypeName<T>::get_hash();

struct TestClass {};

static_assert(type_name<int> == "int");
static_assert(type_name<std::source_location> == "std::source_location");
static_assert(type_name<TestClass> == "ecstl::TestClass");

}