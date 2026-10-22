// OPTIMIZED: the same lookups as Listing 12.7, (1) as a branchless binary search and
// (2) in an Eytzinger (BFS-order) layout with software prefetching (Section 12.8).
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

std::vector<std::uint32_t> make_sorted(std::size_t n, std::mt19937& rng) {
    std::vector<std::uint32_t> a(n);
    std::uint32_t x = 0;
    for (auto& v : a) v = x += 1 + rng() % 7;
    return a;
}

// (1) Branchless: the loop runs exactly ceil(log2 n) times and the comparison feeds an
// arithmetic update (compiled to cmov), so there is nothing to mispredict. Written as
// "cond ? half : 0", g++ 13 turned the same update back into a branch; check the assembly.
std::uint32_t lower_bound_branchless(const std::vector<std::uint32_t>& a,
                                     std::uint32_t x) {
    const std::uint32_t* base = a.data();
    std::size_t len = a.size();
    while (len > 1) {
        const std::size_t half = len / 2;
        base += (base[half - 1] < x) * half;
        len -= half;
    }
    return (*base < x) ? 0 : *base;                     // 0 means "no element >= x"
}

// (2) Eytzinger layout: node k has children 2k and 2k+1, so the first levels of every
// search share a few cache lines and the next levels' addresses are known in advance.
struct Eytzinger {
    std::vector<std::uint32_t> b;                       // b[0] unused
    std::size_t n;
    explicit Eytzinger(const std::vector<std::uint32_t>& sorted)
        : b(sorted.size() + 1), n(sorted.size()) {
        std::size_t i = 0;
        build(sorted, i, 1);
    }
    void build(const std::vector<std::uint32_t>& s, std::size_t& i, std::size_t k) {
        if (k > n) return;                      // in-order walk of the implicit tree
        build(s, i, 2 * k);
        b[k] = s[i++];
        build(s, i, 2 * k + 1);
    }
    std::uint32_t lower_bound(std::uint32_t x) const {
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(b.data());
        std::size_t k = 1;
        while (k <= n) {
            // 16 keys per 64-byte line: node 16k is four levels below k. The address is
            // formed as an integer because it may lie beyond the array, and forming such
            // a pointer would be UB (11.2). Prefetching an unmapped address is harmless.
            __builtin_prefetch(reinterpret_cast<const void*>(base + 16 * k * 4));
            k = 2 * k + (b[k] < x);
        }
        k >>= std::countr_zero(~k) + 1;                 // undo the final run of right turns
        return k ? b[k] : 0;
    }
};

template <class F>
double ns_per_lookup(F f, const std::vector<std::uint32_t>& q, std::uint64_t& check) {
    check = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::uint32_t x : q) check += f(x);
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(q.size());
}

int main() {
    std::mt19937 rng(1);                                // same data as Listing 12.7
    std::printf("%10s %12s %12s %12s\n", "elements", "lower_bound", "branchless",
                "eytzinger");
    for (std::size_t n : {std::size_t(1) << 12, std::size_t(1) << 16, std::size_t(1) << 20,
                          std::size_t(1) << 24}) {
        const auto a = make_sorted(n, rng);
        std::vector<std::uint32_t> q(1 << 19);
        for (auto& x : q) x = rng() % (a.back() + 1);
        const Eytzinger e(a);
        std::uint64_t c0, c1, c2;
        const double t0 = ns_per_lookup([&](std::uint32_t x) {
            auto it = std::lower_bound(a.begin(), a.end(), x);
            return it == a.end() ? 0u : *it; }, q, c0);
        const double t1 = ns_per_lookup([&](std::uint32_t x) {
            return lower_bound_branchless(a, x); }, q, c1);
        const double t2 = ns_per_lookup([&](std::uint32_t x) {
            return e.lower_bound(x); }, q, c2);
        std::printf("%10zu %9.1f ns %9.1f ns %9.1f ns  %s\n", n, t0, t1, t2,
                    (c0 == c1 && c1 == c2) ? "results agree" : "MISMATCH");
    }
}
