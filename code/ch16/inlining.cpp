// What inlining buys (Section 16.2). The same per-element work, written once as a call
// the compiler may inline and once behind [[gnu::noinline]], as if its body were in
// another translation unit. Prints nanoseconds per element over 4,096 elements.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "bench.hpp"

inline std::int32_t to_ticks(std::int32_t px, std::int32_t tick) { return px / tick; }

[[gnu::noinline]] std::int32_t to_ticks_call(std::int32_t px, std::int32_t tick) {
    return px / tick;
}

void convert_inline(const std::int32_t* px, std::int32_t* out, int n) {
    for (int i = 0; i < n; ++i) out[i] = to_ticks(px[i], 5);       // inlined: divide by a
}                                                                    // constant = multiply
void convert_call(const std::int32_t* px, std::int32_t* out, int n) {
    for (int i = 0; i < n; ++i) out[i] = to_ticks_call(px[i], 5);  // a call, and a real
}                                                                    // idiv, per element
template <class F>
double ns_per_element(F f, int n) {
    constexpr int kReps = 20000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < kReps; ++r) f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / kReps / n;
}

int main() {
    constexpr int n = 4096;
    std::vector<std::int32_t> px(n), out(n);
    for (int i = 0; i < n; ++i) px[i] = 100'000 + 5 * i;
    const double a = ns_per_element([&] {
        convert_inline(px.data(), out.data(), n);
        bench::do_not_optimize(out[n - 1]);
    }, n);
    const double b = ns_per_element([&] {
        convert_call(px.data(), out.data(), n);
        bench::do_not_optimize(out[n - 1]);
    }, n);
    std::printf("inlined:      %.3f ns/element\nnot inlined:  %.3f ns/element (%.1fx)\n", a,
                b, b / a);
}
