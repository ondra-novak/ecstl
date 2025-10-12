#pragma once
#include <string_view>

namespace ecstl {

    constexpr std::size_t get_hash(const std::string_view &s) {
        if constexpr(sizeof(std::size_t) == 4) {
            std::size_t h = 2166136261u;
            for (char c : s)
                h = (h ^ static_cast<unsigned char>(c)) * 16777619u;
            return h;
        } else {
            // constexpr FNV-1a 64-bit
            std::size_t h = 1469598103934665603ull;
            for (char c : s)
                h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
            return h;
        }
    }
}
