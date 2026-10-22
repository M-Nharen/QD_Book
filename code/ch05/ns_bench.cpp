// book: timeout=180
// Latency of the arithmetic this chapter cares about, and price-parsing cost (5.10).
// Each loop is a dependency chain: every operation needs the previous result, so the
// time per iteration is the operation's latency, not its throughput.
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

template <class T> inline void opaque(T& x) {    // hide x's value from the optimizer
    if constexpr (sizeof(T) == 8 && T(0.5) != 0) asm volatile("" : "+x"(x));
    else asm volatile("" : "+r"(x));
}

volatile std::int64_t v_div = 1000;              // values the compiler cannot see
volatile std::int32_t v_div32 = 1000;
volatile double v_one = 1.0, v_small = 1e-3;
volatile std::uint64_t sink;

template <class T, class Step>
void chain(const char* name, T x0, Step step) {
    const long n = 50'000'000;
    T x = x0;
    const auto t0 = std::chrono::steady_clock::now();
    for (long i = 0; i < n; ++i) { x = step(x); opaque(x); }
    const auto t1 = std::chrono::steady_clock::now();
    std::uint64_t bits;
    std::memcpy(&bits, &x, sizeof bits < sizeof x ? sizeof bits : sizeof x);
    sink = bits;
    std::printf("  %-46s %6.2f ns\n", name,
                std::chrono::duration<double, std::nano>(t1 - t0).count() / n);
}

inline std::int64_t parse_cents(const char* s) {     // as in Listing 5.10
    std::int64_t v = 0;
    for (; *s >= '0' && *s <= '9'; ++s) v = v * 10 + (*s - '0');
    if (s[0] != '.' || s[1] < '0' || s[1] > '9' || s[2] < '0' || s[2] > '9' || s[3])
        return -1;
    return v * 100 + (s[1] - '0') * 10 + (s[2] - '0');
}

template <class F>
void parse_bench(const char* name, const std::vector<std::array<char, 16>>& in, F f) {
    const int reps = 20;
    double acc = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; ++r)
        for (const auto& s : in) acc += f(s.data());
    const auto t1 = std::chrono::steady_clock::now();
    sink = static_cast<std::uint64_t>(acc);
    std::printf("  %-46s %6.2f ns\n", name,
                std::chrono::duration<double, std::nano>(t1 - t0).count() /
                    (reps * double(in.size())));
}

int main() {
    const std::int64_t d = v_div;
    const std::uint64_t ud = static_cast<std::uint64_t>(d);   // unsigned: wraps, no UB
    const std::int32_t d32 = v_div32;
    const double one = v_one, small = v_small;
    constexpr std::int64_t hi = std::int64_t{1} << 40;   // keeps dividends ~1e12

    std::printf("Latency per dependent operation (ns):\n");
    chain("int64 add", std::int64_t{1}, [&](std::int64_t x) { return x + d; });
    chain("uint64 multiply", std::uint64_t{3}, [&](std::uint64_t x) { return x * ud; });
    chain("int64 divide by runtime value, then OR", hi,
          [&](std::int64_t x) { return (x / d) | hi; });
    chain("int32 divide by runtime value, then OR", std::int32_t{1 << 30},
          [&](std::int32_t x) { return (x / d32) | (1 << 30); });
    chain("int64 divide by constant 1000, then OR", hi,
          [&](std::int64_t x) { return (x / 1000) | hi; });
    chain("int64 shift then OR (baseline for the above)", hi,
          [&](std::int64_t x) { return (x >> 1) | hi; });
    chain("double add", 1.0, [&](double x) { return x + small; });
    chain("double multiply", 1.0, [&](double x) { return x * one; });
    chain("double divide", 1.0, [&](double x) { return x / one; });
    chain("double multiply, subnormal operand", 1e-310, [&](double x) { return x * one; });
    chain("double add, subnormal operands", 1e-310,
          [&](double x) { return x + 1e-320 * one; });

    std::vector<std::array<char, 16>> in(1'000'000);
    std::uint64_t r = 7;
    for (auto& s : in) {
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
        const long long c = 10'000 + static_cast<long long>((r >> 33) % 1'000);
        std::snprintf(s.data(), s.size(), "%lld.%02lld", c / 100, c % 100);
    }
    std::printf("Parsing a price like \"101.37\" (ns per call):\n");
    parse_bench("std::strtod", in, [](const char* s) { return std::strtod(s, nullptr); });
    parse_bench("std::from_chars (double)", in, [](const char* s) {
        double v = 0;
        std::from_chars(s, s + std::strlen(s), v);
        return v;
    });
    parse_bench("hand-written fixed-point parse_cents", in,
                [](const char* s) { return double(parse_cents(s)); });
}
