// Two ways to turn a decimal price string such as "101.2575" into an integer number of
// 1/10,000ths (fixed-point, 5.7). The subject of the benchmarking showdown (Section 16.7).
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>

inline std::int64_t parse_strtod(const char* s) {     // via double: locale-aware, general
    return std::llround(std::strtod(s, nullptr) * 10'000.0);
}

inline std::int64_t parse_fixed(const char* s) {      // digits only, at most 4 decimals
    std::int64_t v = 0;
    int decimals = -1;                                 // -1: no '.' seen yet
    for (; *s; ++s) {
        if (*s == '.') { decimals = 0; continue; }
        v = v * 10 + (*s - '0');
        decimals += decimals >= 0;
    }
    for (decimals = decimals < 0 ? 0 : decimals; decimals < 4; ++decimals) v *= 10;
    return v;
}
