// The nanosecond penalty of Chapter 3's building blocks (3.11). Timings depend on the CPU,
// its cache sizes and system load; read the text before quoting any number.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <random>
#include <unordered_map>
#include <vector>
using Clock = std::chrono::steady_clock;
static std::uint64_t sink = 0;                     // printed at the end: keeps work alive

// Branch-free lower_bound: the trip count depends only on n, and the comparison result is
// used as a number (0 or 1) rather than as a jump condition: nothing to mispredict.
std::size_t lower_bound_branchless(const std::vector<int>& a, int x) {
    const int* base = a.data();
    std::size_t len = a.size();
    while (len > 1) {
        const std::size_t half = len / 2;
        base += half * std::size_t(base[half - 1] < x);   // setl + multiply, no jump
        len -= half;
    }
    return std::size_t(base - a.data()) + std::size_t(len == 1 && *base < x);
}

template <class F>
double ns_per_op(long long ops, F f) {
    const auto t0 = Clock::now();
    f();
    const auto t1 = Clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(ops);
}

int main() {
    std::mt19937_64 rng(11);
    const int Q = 1 << 20;                         // queries per measurement

    std::printf("1) std::lower_bound on a sorted int array, random queries\n");
    for (int lg : {10, 16, 20, 24, 26}) {
        const std::size_t n = std::size_t(1) << lg;
        std::vector<int> a(n);
        for (std::size_t i = 0; i < n; ++i) a[i] = int(2 * i);
        std::vector<int> q(Q);
        for (int& x : q) x = int(rng() % (2 * n));
        const double t1 = ns_per_op(Q, [&] {
            for (int x : q)
                sink += std::size_t(std::lower_bound(a.begin(), a.end(), x) - a.begin());
        });
        const double t2 = ns_per_op(Q, [&] {
            for (int x : q) sink += lower_bound_branchless(a, x);
        });
        std::printf("   n = 2^%-2d (%6.1f MiB): std %6.1f ns, branch-free %6.1f ns\n", lg,
                    double(n * sizeof(int)) / (1 << 20), t1, t2);
    }

    std::printf("2) find among 10^6 random 64-bit keys (all queries hit)\n");
    const int K = 1'000'000;
    std::vector<std::uint64_t> keys(K);
    for (auto& k : keys) k = rng();
    std::vector<std::uint64_t> sorted = keys;
    std::sort(sorted.begin(), sorted.end());
    std::unordered_map<std::uint64_t, int> um;
    um.reserve(K);
    for (int i = 0; i < K; ++i) um[keys[i]] = i;
    std::vector<std::uint64_t> qk(Q);
    for (auto& k : qk) k = keys[rng() % K];
    std::printf("   unordered_map::find    : %6.1f ns\n", ns_per_op(Q, [&] {
        for (auto k : qk) sink += std::uint64_t(um.find(k)->second);
    }));
    std::printf("   sorted vector + search : %6.1f ns\n", ns_per_op(Q, [&] {
        for (auto k : qk)
            sink += std::uint64_t(std::lower_bound(sorted.begin(), sorted.end(), k)
                                  - sorted.begin());
    }));
    std::vector<int> dense(K);                     // when keys are small integers: an array
    std::vector<int> qi(Q);
    for (int& x : qi) x = int(rng() % K);
    std::printf("   plain array index      : %6.1f ns\n", ns_per_op(Q, [&] {
        for (int x : qi) sink += std::uint64_t(dense[x]);
    }));

    std::printf("3) std::priority_queue<int> holding 10^6 elements: one push + one pop\n");
    std::priority_queue<int> pq;
    for (int i = 0; i < K; ++i) pq.push(int(rng() >> 33));
    std::vector<int> vals(Q);
    for (int& v : vals) v = int(rng() >> 33);
    std::printf("   push + pop             : %6.1f ns\n", ns_per_op(Q, [&] {
        for (int v : vals) { pq.push(v); sink += std::uint64_t(pq.top()); pq.pop(); }
    }));

    std::printf("4) dependent chain h = (h ^ x) %% m: divisor constant vs run-time\n");
    std::vector<std::uint64_t> xs(Q);
    for (auto& x : xs) x = rng();
    volatile std::uint64_t runtime_m = 1'000'000'007ULL;   // the compiler cannot see it
    const std::uint64_t m = runtime_m;
    std::uint64_t h = 1;                           // each step needs the previous result
    std::printf("   %% 1000000007 (constant): %6.2f ns\n", ns_per_op(Q, [&] {
        for (auto x : xs) h = (h ^ x) % 1'000'000'007ULL;
    }));
    std::printf("   %% m (run-time divisor) : %6.2f ns\n", ns_per_op(Q, [&] {
        for (auto x : xs) h = (h ^ x) % m;
    }));
    sink += h;
    std::printf("(checksum %llu)\n", static_cast<unsigned long long>(sink % 1000));
}
