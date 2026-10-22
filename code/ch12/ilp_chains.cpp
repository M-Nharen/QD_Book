// Latency versus throughput (Section 12.1). Summing doubles with one accumulator makes
// every addition wait for the previous one; with k accumulators the out-of-order core can
// keep k additions in flight. The second part times dependent chains of 64-bit multiply
// and divide, the cheapest and the most expensive common integer operations.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

template <int K>
[[gnu::noipa]] double sum_k(const double* a, std::size_t n) {
    double acc[K] = {};
    for (std::size_t i = 0; i < n; i += K) {
#pragma GCC unroll 16
        for (int j = 0; j < K; ++j) acc[j] += a[i + j];         // K independent chains
    }
    double s = 0;
    for (int j = 0; j < K; ++j) s += acc[j];
    return s;
}

template <class F>
double best_ns(F&& f, double per_call, int reps) {             // best of 9 trials
    double best = 1e300;
    for (int t = 0; t < 9; ++t) {
        auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < reps; ++r) f();
        auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        if (ns < best) best = ns;
    }
    return best / (per_call * reps);
}

[[gnu::noipa]] std::uint64_t mul_chain(std::uint64_t x, std::uint64_t m, int n) {
    for (int i = 0; i < n; ++i) x = x * m;                      // imul: each needs the last
    return x;
}
[[gnu::noipa]] std::uint64_t div_chain(std::uint64_t x, std::uint64_t d, int n) {
    for (int i = 0; i < n; ++i) x = x / d + 0x9e3779b97f4a7c15ULL;     // div r64
    return x;
}

int main() {
    constexpr std::size_t N = 2048;                             // 16 KiB: fits in L1
    std::vector<double> a(N);
    for (std::size_t i = 0; i < N; ++i) a[i] = 1.0 / double(i + 1);
    volatile double sink = 0;
    std::printf("double sum, ns per element (data in L1):\n");
    const char* label[] = {"1 accumulator ", "2 accumulators", "4 accumulators",
                           "8 accumulators"};
    const double t[] = {best_ns([&] { sink = sum_k<1>(a.data(), N); }, N, 2000),
                        best_ns([&] { sink = sum_k<2>(a.data(), N); }, N, 2000),
                        best_ns([&] { sink = sum_k<4>(a.data(), N); }, N, 2000),
                        best_ns([&] { sink = sum_k<8>(a.data(), N); }, N, 2000)};
    for (int i = 0; i < 4; ++i) std::printf("  %s %.3f\n", label[i], t[i]);
    volatile std::uint64_t seed = 0x123456789abcdefULL, m = 0x9e3779b97f4a7c15ULL, d = 7;
    volatile std::uint64_t out = 0;
    const int n = 100000;
    std::printf("dependent chains, ns per operation:\n");
    const double tm = best_ns([&] { out = mul_chain(seed, m, n); }, n, 20);
    const double td = best_ns([&] { out = div_chain(seed, d, n); }, n, 20);
    std::printf("  64-bit multiply %.3f\n  64-bit divide   %.3f\n", tm, td);
}
