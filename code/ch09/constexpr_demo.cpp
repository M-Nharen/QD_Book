// constexpr, consteval, constinit and std::is_constant_evaluated (Section 9.2).
#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <type_traits>
#include <vector>

// constexpr: CAN run at compile time; runs at run time when arguments are not constants.
constexpr std::uint64_t pow10(int n) {
    std::uint64_t r = 1;
    while (n-- > 0) r *= 10;
    return r;
}

// A lookup table built by the compiler. It ends up as 160 bytes of read-only data.
constexpr auto kPow10 = [] {
    std::array<std::uint64_t, 20> t{};
    for (int i = 0; i < 20; ++i) t[i] = pow10(i);
    return t;
}();
static_assert(kPow10[0] == 1 && kPow10[19] == 10'000'000'000'000'000'000ULL);

// FNV-1a: usable at compile time and at run time.
constexpr std::uint32_t fnv1a(std::string_view s) {
    std::uint32_t h = 2166136261u;
    for (char c : s) { h ^= static_cast<unsigned char>(c); h *= 16777619u; }
    return h;
}
// consteval: every call MUST be a constant expression (an "immediate function").
consteval std::uint32_t type_id(std::string_view s) { return fnv1a(s); }

const char* classify(std::string_view name) {
    switch (fnv1a(name)) {                    // run-time hash of run-time data ...
    case type_id("NewOrder"): return name == "NewOrder" ? "new" : "?";   // ... against
    case type_id("Cancel"):   return name == "Cancel" ? "cancel" : "?";  //  compile-time
    case type_id("Replace"):  return name == "Replace" ? "replace" : "?"; // labels
    default:                  return "unknown";
    }
}

// C++20: transient heap allocation is allowed during constant evaluation.
constexpr int count_primes_below(int n) {
    std::vector<bool> composite(n, false);
    int count = 0;
    for (int i = 2; i < n; ++i) {
        if (composite[i]) continue;
        ++count;
        for (long long j = 1LL * i * i; j < n; j += i) composite[j] = true;
    }
    return count;                             // the vector is freed before evaluation ends
}
static_assert(count_primes_below(1000) == 168);

// constinit: static storage, initialized at compile time, but NOT const.
constinit std::uint64_t g_next_order_id = pow10(6);
// constinit int g_bad = std::rand();         // error: not a constant initializer

constexpr bool in_constant_evaluation() { return std::is_constant_evaluated(); }

int main(int argc, char**) {
    std::printf("pow10(argc + 2) = %llu (computed at run time)\n",
                static_cast<unsigned long long>(pow10(argc + 2)));
    std::printf("kPow10[9]      = %llu (read from the table)\n",
                static_cast<unsigned long long>(kPow10[9]));
    for (std::string_view s : {"Cancel", "NewOrder", "Trade"})
        std::printf("classify(%-8.*s) = %s\n", int(s.size()), s.data(), classify(s));
    g_next_order_id += 1;                     // mutable: constinit is not const
    std::printf("g_next_order_id = %llu\n",
                static_cast<unsigned long long>(g_next_order_id));

    constexpr bool a = in_constant_evaluation();  // manifestly constant-evaluated
    const bool b = in_constant_evaluation();      // const bool: also constant-evaluated!
    bool c = in_constant_evaluation();            // ordinary variable: run time
    std::printf("is_constant_evaluated: constexpr=%d const=%d plain=%d\n", a, b, c);
}
