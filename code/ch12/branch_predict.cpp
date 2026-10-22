// Branch prediction (Section 12.5): the same loop over the same values, in random order,
// sorted, and in a short repeating pattern; with a branch and without one.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

struct Result { long long sum, count; };

// Branchy: g++ keeps this as a conditional jump because two variables are updated.
[[gnu::noipa]] Result sum_big_branchy(const std::vector<std::int32_t>& v) {
    long long s = 0, c = 0;
    for (std::int32_t x : v)
        if (x >= 128) { s += x; ++c; }
    return {s, c};
}

// Branchless: turn the condition into a 0/-1 mask and use it arithmetically.
[[gnu::noipa]] Result sum_big_branchless(const std::vector<std::int32_t>& v) {
    long long s = 0, c = 0;
    for (std::int32_t x : v) {
        const long long take = -static_cast<long long>(x >= 128);   // 0 or all ones
        s += x & take;
        c -= take;
    }
    return {s, c};
}

template <class F>
double ns_per_elem(F f, const std::vector<std::int32_t>& v, long long& sink) {
    double best = 1e300;
    for (int t = 0; t < 7; ++t) {                       // best of 7 trials
        auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < 50; ++r) { Result x = f(v); sink += x.sum + x.count; }
        auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    return best / (50.0 * double(v.size()));
}

int main() {
    constexpr std::size_t N = 1 << 16;
    std::mt19937 rng(12);
    std::vector<std::int32_t> random(N);
    for (auto& x : random) x = std::int32_t(rng() % 256);
    std::vector<std::int32_t> sorted = random;
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::int32_t> pattern(N);               // taken, taken, not, not, ...
    for (std::size_t i = 0; i < N; ++i) pattern[i] = (i % 4 < 2) ? 200 : 50;

    long long sink = 0;
    std::printf("%-22s %10s %12s\n", "data", "branchy", "branchless");
    const std::vector<std::int32_t>* data[] = {&random, &sorted, &pattern};
    const char* name[] = {"random (50% taken)", "sorted", "pattern TTNN"};
    for (int k = 0; k < 3; ++k) {
        const double b = ns_per_elem(sum_big_branchy, *data[k], sink);
        const double f = ns_per_elem(sum_big_branchless, *data[k], sink);
        std::printf("%-22s %7.3f ns %9.3f ns\n", name[k], b, f);
    }
    std::printf("(checksum %lld)\n", sink);
}
