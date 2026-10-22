// Stress testing: run a fast solution against a brute force on tiny random inputs (3.2).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
using ll = long long;
using Vec = std::vector<ll>;

ll brute(const Vec& a) {                           // O(n^2): try every subarray
    ll best = a[0];
    for (std::size_t i = 0; i < a.size(); ++i) {
        ll s = 0;
        for (std::size_t j = i; j < a.size(); ++j) best = std::max(best, s += a[j]);
    }
    return best;
}
ll kadane_buggy(const Vec& a) {                    // looks right, passes the sample tests
    ll best = 0, cur = 0;
    for (ll v : a) {
        cur = std::max(cur + v, 0LL);
        best = std::max(best, cur);
    }
    return best;
}
ll kadane(const Vec& a) {
    ll best = a[0], cur = 0;
    for (ll v : a) {
        cur = std::max(cur + v, v);
        best = std::max(best, cur);
    }
    return best;
}

template <class Fast>
void stress(const char* name, Fast fast, int trials, std::uint64_t seed) {
    std::mt19937_64 rng(seed);                     // fixed seed: failures are reproducible
    std::uniform_int_distribution<int> len(1, 6), val(-5, 5);
    for (int t = 0; t < trials; ++t) {
        Vec a(len(rng));                           // tiny cases are easy to debug by hand
        for (ll& v : a) v = val(rng);
        const ll want = brute(a), got = fast(a);
        if (want != got) {
            std::printf("%-13s FAILS on trial %d: a = [", name, t);
            for (std::size_t i = 0; i < a.size(); ++i)
                std::printf("%s%lld", i ? ", " : "", a[i]);
            std::printf("]  expected %lld, got %lld\n", want, got);
            return;
        }
    }
    std::printf("%-13s passed %d random tests\n", name, trials);
}

int main() {
    const Vec sample{-2, 1, -3, 4, -1, 2, 1, -5, 4};   // the usual sample: answer 6
    std::printf("sample: brute %lld, buggy %lld, kadane %lld\n",
                brute(sample), kadane_buggy(sample), kadane(sample));
    stress("kadane_buggy", kadane_buggy, 100000, 42);
    stress("kadane", kadane, 100000, 42);
}
