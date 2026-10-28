// Lab kernel for Section 16.1: calls notional() from lto_lib.cc in a hot loop. Without
// -flto the call cannot be inlined, because its body is in another translation unit.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

std::int64_t notional(std::int32_t px, std::int32_t qty);

int main() {
    constexpr int n = 4096, kReps = 20000;
    std::vector<std::int32_t> px(n, 3), qty(n, 5);
    std::int64_t total = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < kReps; ++r) {
        for (int i = 0; i < n; ++i) total += notional(px[i], qty[i]);
        asm volatile("" : "+r"(total));               // keep the loop from being folded
    }
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("%.3f\n", std::chrono::duration<double, std::nano>(t1 - t0).count() /
                              kReps / n + 0 * double(total));
}
