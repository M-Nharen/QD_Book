// Checks every numeric or yes/no answer in the Chapter 5 solutions (Appendix A).
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

static int failures = 0;
template <class A, class B>
void check(const char* what, A got, B want) {
    const bool ok = got == want;
    failures += !ok;
    std::printf("%-58s %s\n", what, ok ? "ok" : "MISMATCH");
}

struct S3 { std::string s; ~S3() = default; };   // P5.3: "= default" still declares it
struct S3b { std::string s; };

std::int64_t floor_avg(std::int64_t a, std::int64_t b) { return (a & b) + ((a ^ b) >> 1); }

int main() {
    // P5.1
    std::uint8_t a = 250, b = 10;
    auto c = a + b;
    std::uint8_t d = static_cast<std::uint8_t>(a + b);
    check("P5.1 a+b is int", std::is_same_v<decltype(c), int>, true);
    check("P5.1 c == 260", c, 260);
    check("P5.1 d == 4", int(d), 4);

    // P5.3
    check("P5.3 S3 has a move ctor that cannot throw?",
          std::is_nothrow_move_constructible_v<S3>, false);
    check("P5.3 S3b (no destructor declared) can?",
          std::is_nothrow_move_constructible_v<S3b>, true);

    // P5.4
    int m7 = -7, p7 = 7, m2 = -2;
    check("P5.4 -7/2", m7 / 2, -3);
    check("P5.4 -7%2", m7 % 2, -1);
    check("P5.4 7/-2", p7 / m2, -3);
    check("P5.4 7%-2", p7 % m2, 1);
    check("P5.4 -7>>1", m7 >> 1, -4);

    // P5.5
    const std::string s1(3, 'x'), s2{3, 'x'};
    check("P5.5 string(3, 'x') == \"xxx\"", s1 == "xxx", true);
    check("P5.5 string{3, 'x'} holds char(3), 'x'", s2.size() == 2 && s2[0] == 3, true);
    const std::vector<int> v10(10, 0), v2{10, 0};
    check("P5.5 vector(10, 0).size()", v10.size(), 10u);
    check("P5.5 vector{10, 0}.size()", v2.size(), 2u);

    // P5.8: smallest x with 1 + x != 1, and the exact-integer limits
    const double half_ulp = std::ldexp(1.0, -53);
    check("P5.8 1 + 2^-53 == 1 (tie rounds to even)", 1.0 + half_ulp == 1.0, true);
    const double x = std::nextafter(half_ulp, 1.0);
    check("P5.8 1 + nextafter(2^-53, 1) != 1", 1.0 + x != 1.0, true);
    check("P5.8 x == 2^-53 + 2^-105", x, half_ulp + std::ldexp(1.0, -105));
    check("P5.8 DBL_EPSILON == 2^-52", DBL_EPSILON, std::ldexp(1.0, -52));
    const float f24 = 16777216.0f;               // 2^24
    check("P5.8 float: 2^24 + 1 not representable", f24 + 1.0f == f24, true);
    check("P5.8 float: 2^24 - 1 representable", float(16777215) == 16777215.0f, true);
    const double d53 = 9007199254740992.0;       // 2^53
    check("P5.8 double: 2^53 + 1 not representable", d53 + 1.0 == d53, true);

    // P5.9: k*0.05 written as a decimal string, parsed, divided by the tick, truncated
    int wrong_cast = 0, wrong_round = 0;
    for (int k = 1; k <= 20'000; ++k) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%d.%02d", k * 5 / 100, k * 5 % 100);
        const double px = std::strtod(buf, nullptr);
        wrong_cast += static_cast<std::int64_t>(px / 0.05) != k;
        wrong_round += std::llround(px / 0.05) != k;
    }
    std::printf("P5.9 truncation wrong for %d of 20000 prices\n", wrong_cast);
    check("P5.9 truncation wrong count", wrong_cast, 6'457);
    check("P5.9 llround wrong count", wrong_round, 0);

    // P5.10: floor average without overflow, against 128-bit arithmetic
    bool all_ok = true;
    std::uint64_t r = 1;
    for (int i = 0; i < 1'000'000; ++i) {
        r ^= r << 13; r ^= r >> 7; r ^= r << 17;
        const std::int64_t p = static_cast<std::int64_t>(r);
        r ^= r << 13; r ^= r >> 7; r ^= r << 17;
        const std::int64_t q = static_cast<std::int64_t>(r);
        const __int128 s = static_cast<__int128>(p) + q;
        const __int128 fl = s >= 0 ? s / 2 : -((-s + 1) / 2);  // floor(s / 2)
        all_ok &= floor_avg(p, q) == static_cast<std::int64_t>(fl);
    }
    for (int p = -300; p <= 300; ++p)
        for (int q = -300; q <= 300; ++q) {
            const int s = p + q;
            const int fl = s >= 0 ? s / 2 : -((-s + 1) / 2);
            all_ok &= floor_avg(p, q) == fl;
        }
    check("P5.10 (a&b)+((a^b)>>1) == floor((a+b)/2)", all_ok, true);

    // P5.13: allocation count for QuoteDtor in Listing 5.5, N = 200000
    const long N = 200'000;
    long caps = 0, buffers = 0;
    for (long cap = 1; ; cap *= 2) {             // vector capacities 1, 2, 4, ...
        ++buffers;
        if (cap >= N) break;
        caps += cap;                             // elements copied when this one is full
    }
    check("P5.13 QuoteZero allocations", 2 * N + buffers, 400'019L);
    check("P5.13 QuoteDtor allocations", 2 * N + buffers + 2 * N + 2 * caps, 1'324'305L);

    // P5.12
    std::vector<int> v{-3, 1};
    int total = 0;
    for (int e : v) total += e;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
    const double avg = total / v.size();         // the bug under discussion
#pragma GCC diagnostic pop
    std::printf("P5.12 avg = %.17g\n", avg);
    check("P5.12 avg == 2^63 (after rounding to double)", avg, std::ldexp(1.0, 63));

    std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
    return failures != 0;
}
