// Integer representation, promotions and conversions on x86-64 Linux (Section 5.6).
#include <bit>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

int main() {
    std::printf("sizes: int %zu, long %zu, long long %zu, size_t %zu, pointer %zu\n",
                sizeof(int), sizeof(long), sizeof(long long), sizeof(std::size_t),
                sizeof(void*));

    std::uint8_t a = 200, b = 100;
    auto sum = a + b;                            // both promoted to int first
    std::uint8_t stored = static_cast<std::uint8_t>(a + b);    // 300 mod 256
    std::printf("uint8 200+100: %d (sizeof %zu), stored in uint8: %d\n", sum, sizeof(sum),
                stored);

    int neg = -1;
    unsigned one = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"  // to show what -Wall protects you from
    std::printf("-1 < 1u: %s\n", neg < one ? "true" : "false");   // -1 becomes 4294967295
#pragma GCC diagnostic pop
    std::printf("std::cmp_less(-1, 1u): %s\n", std::cmp_less(neg, one) ? "true" : "false");

    std::vector<int> empty;
    std::printf("empty.size() - 1 = %zu\n", empty.size() - 1);        // wraps, no UB

    std::printf("bits of -1 as uint32: 0x%08x, INT_MIN: 0x%08x\n",
                std::bit_cast<std::uint32_t>(-1), std::bit_cast<std::uint32_t>(INT_MIN));
    std::int64_t big = 5'000'000'000;
    std::printf("int64 5e9 -> int32: %d (modulo 2^32, defined since C++20)\n",
                static_cast<std::int32_t>(big));

    int m = -7;
    std::printf("-7 / 2 = %d, -7 %% 2 = %d, -7 >> 1 = %d\n", m / 2, m % 2, m >> 1);

    std::int32_t px_ticks = 150'000, qty = 20'000;   // each fits comfortably in int32
    std::int32_t notional32;
    bool overflow = __builtin_mul_overflow(px_ticks, qty, &notional32);
    std::int64_t notional64 = std::int64_t{px_ticks} * qty;   // widen BEFORE multiplying
    std::printf("150000 * 20000: overflows int32? %s; as int64: %lld\n",
                overflow ? "yes" : "no", static_cast<long long>(notional64));
}
