// Bit-manipulation idioms with C++20 <bit> (3.8).
#include <bit>
#include <cstdint>
#include <cstdio>
#include <vector>
using u32 = std::uint32_t;
using u64 = std::uint64_t;

int main() {
    const u32 x = 0b1011'0100;                     // 180
    std::printf("x            = %u\n", x);
    std::printf("x & (x - 1)  = %u   (clear lowest set bit)\n", x & (x - 1));
    std::printf("x & -x       = %u   (isolate lowest set bit)\n", x & (0u - x));
    std::printf("popcount     = %d\n", std::popcount(x));
    std::printf("countr_zero  = %d   (index of lowest set bit)\n", std::countr_zero(x));
    std::printf("bit_width    = %d   (floor(log2 x) + 1)\n", std::bit_width(x));
    std::printf("bit_ceil     = %u\n", std::bit_ceil(x));
    std::printf("single bit?  64: %d, 96: %d\n", std::has_single_bit(64u),
                std::has_single_bit(96u));

    // Iterate over the set bits of a mask: O(popcount), not O(width).
    std::printf("set bits of x:");
    for (u32 m = x; m != 0; m &= m - 1) std::printf(" %d", std::countr_zero(m));
    std::printf("\n");

    // All subsets of {0..n-1} as masks, and every submask of every mask: 3^n in total.
    const int n = 10;
    long long pairs = 0;
    for (u32 mask = 0; mask < (1u << n); ++mask) {
        for (u32 s = mask;; s = (s - 1) & mask) {  // submasks of mask, largest first
            ++pairs;
            if (s == 0) break;                     // the empty submask is visited last
        }
    }
    std::printf("submask pairs for n=%d: %lld (3^n = 59049)\n", n, pairs);

    // XOR: every value appears twice except one.
    const std::vector<u64> v{4, 9, 7, 9, 4, 1, 7};
    u64 acc = 0;
    for (u64 e : v) acc ^= e;
    std::printf("unpaired element: %llu\n", static_cast<unsigned long long>(acc));

    // Gray code: consecutive values differ in exactly one bit.
    std::printf("gray codes:");
    for (u32 i = 0; i < 8; ++i) std::printf(" %u", i ^ (i >> 1));
    std::printf("\n");
}
