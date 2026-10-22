// NAIVE: lower_bound lookups in a sorted array (Section 12.8). Typical use: map a price,
// an instrument id or a timestamp to its position in a large sorted table.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

std::vector<std::uint32_t> make_sorted(std::size_t n, std::mt19937& rng) {
    std::vector<std::uint32_t> a(n);
    std::uint32_t x = 0;
    for (auto& v : a) v = x += 1 + rng() % 7;          // strictly increasing, gaps 1..7
    return a;
}

int main() {
    std::mt19937 rng(1);
    std::printf("%10s %12s %14s\n", "elements", "array size", "ns per lookup");
    for (std::size_t n : {std::size_t(1) << 12, std::size_t(1) << 16, std::size_t(1) << 20,
                          std::size_t(1) << 24}) {
        const auto a = make_sorted(n, rng);
        std::vector<std::uint32_t> q(1 << 19);
        for (auto& x : q) x = rng() % (a.back() + 1);
        std::uint64_t check = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (std::uint32_t x : q) {
            auto it = std::lower_bound(a.begin(), a.end(), x);   // branchy binary search
            check += (it == a.end()) ? 0 : *it;
        }
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        std::printf("%10zu %9zu KiB %14.1f   (check %llu)\n", n, n * 4 / 1024,
                    ns / double(q.size()), (unsigned long long)check);
    }
}
