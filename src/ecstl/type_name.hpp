#pragma once
#include <string_view>
#include <source_location>
namespace ecstl {


template<typename __TypeNameArgument__>
struct GenerateTypeName {
    static constexpr std::string_view get_class_name() {
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
            pos = sn1.find(">::get_class_name");
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

    
};

template<typename T>
static constexpr std::string_view type_name = GenerateTypeName<T>::get_class_name();

}