// Checks the numeric answers of the Chapter 11 practice problems (Appendix A).
// Every expression evaluated here has defined behaviour in C++20.
#include <bit>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

static int failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond); ++failures; } \
    } while (0)

// P11.2: subtraction overflow test without UB.
constexpr bool sub_would_overflow(int a, int b) {
    return (b < 0 && a > INT_MAX + b) || (b > 0 && a < INT_MIN + b);
}
// P11.4: big-endian 32-bit load.
std::uint32_t load_be32(const unsigned char* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
}
std::uint32_t load_le32(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);                   // host is little-endian (x86-64)
    return v;
}
// P11.6: magnitude and distance without UB.
constexpr unsigned unsigned_abs(int x) { return x < 0 ? 0u - unsigned(x) : unsigned(x); }
constexpr unsigned abs_diff(int a, int b) {
    return a > b ? unsigned(a) - unsigned(b) : unsigned(b) - unsigned(a);
}
// P11.9: string hash in unsigned arithmetic.
std::uint32_t hash31(std::string_view s) {
    std::uint32_t h = 0;
    for (unsigned char c : s) h = h * 31u + c;
    return h;
}
// P11.10: overflow-checked 64-bit multiply, no builtins, no 128-bit types.
bool mul_overflows(std::int64_t a, std::int64_t b) {
    if (a == 0 || b == 0) return false;
    if (a > 0) {
        if (b > 0) return a > INT64_MAX / b;
        return b < INT64_MIN / a;                         // a > 0, b < 0
    }
    if (b > 0) return a < INT64_MIN / b;                  // a < 0, b > 0
    return a < INT64_MAX / b;                             // both negative
}

int main() {
    // P11.1 values for the snippets with defined behaviour.
    unsigned u = 0; --u;
    CHECK(u == 4294967295u);
    CHECK((1 << 31) == INT_MIN);
    CHECK((-7 >> 1) == -4);
    CHECK(static_cast<char>(300) == 44);
    CHECK(-7 % 2 == -1 && -7 / 2 == -3);
    // P11.2
    const int edge[] = {INT_MIN, INT_MIN + 1, -2, -1, 0, 1, 2, INT_MAX - 1, INT_MAX};
    for (int a : edge)
        for (int b : edge) {
            const long long w = (long long)a - b;
            CHECK(sub_would_overflow(a, b) == (w < INT_MIN || w > INT_MAX));
        }
    // P11.3
    CHECK(std::bit_cast<std::uint64_t>(-0.0) == 0x8000000000000000ULL);
    CHECK(std::bit_cast<std::uint64_t>(1.0) == 0x3ff0000000000000ULL);
    CHECK(std::bit_cast<std::uint64_t>(-2.5) == 0xc004000000000000ULL);
    // P11.4
    const unsigned char b4[4] = {0x10, 0x27, 0x00, 0x00};
    CHECK(load_le32(b4) == 10000u);
    CHECK(load_be32(b4) == 270991360u);
    // P11.6
    CHECK(unsigned_abs(INT_MIN) == 2147483648u);
    CHECK(abs_diff(INT_MIN, INT_MAX) == 4294967295u);
    CHECK(abs_diff(-3, 4) == 7u);
    // P11.9
    CHECK(hash31("AAPL") == 2001436u);
    CHECK(hash31("ESZ6 C5000") == 3524906312u);
    // P11.10: compare with the GCC builtin on edge values and a pseudo-random sweep.
    const std::int64_t e64[] = {INT64_MIN, INT64_MIN + 1, -3037000500LL, -3037000499LL,
                                -2, -1, 0, 1, 2, 3037000499LL, 3037000500LL,
                                INT64_MAX - 1, INT64_MAX};
    long long tested = 0;
    for (std::int64_t a : e64)
        for (std::int64_t b : e64) {
            std::int64_t r;
            CHECK(mul_overflows(a, b) == __builtin_mul_overflow(a, b, &r));
            ++tested;
        }
    std::uint64_t s = 99;
    for (int i = 0; i < 2000000; ++i) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        const std::int64_t a = std::int64_t(s) >> (s & 63);
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        const std::int64_t b = std::int64_t(s) >> (s & 63);
        std::int64_t r;
        CHECK(mul_overflows(a, b) == __builtin_mul_overflow(a, b, &r));
        ++tested;
    }
    std::printf("P11.10: %lld products checked against __builtin_mul_overflow\n", tested);
    std::printf("hash31(\"ESZ6 C5000\") = %u\n", hash31("ESZ6 C5000"));
    std::printf("%s (%d failures)\n", failures ? "SOME CHECKS FAILED" : "all checks passed",
                failures);
    return failures != 0;
}
