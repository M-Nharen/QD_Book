// Lab kernel for Section 16.6: an allocation- and pointer-heavy workload (hash map
// inserts and lookups, then a sort) used to measure what each sanitizer costs.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <unordered_map>
#include <vector>

int main() {
    constexpr int n = 200'000;
    std::mt19937_64 rng(6);
    std::vector<std::uint64_t> keys(n);
    for (auto& k : keys) k = rng();
    const auto t0 = std::chrono::steady_clock::now();
    std::unordered_map<std::uint64_t, std::uint64_t> m;
    for (int i = 0; i < n; ++i) m[keys[i]] = i;
    std::uint64_t hits = 0;
    for (int r = 0; r < 4; ++r)
        for (auto k : keys) hits += m.count(k ^ (r & 1));
    std::sort(keys.begin(), keys.end());
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("%.3f\n", std::chrono::duration<double, std::nano>(t1 - t0).count() /
                              (n * 6.0) + 0 * double(hits + keys[0]));
}
