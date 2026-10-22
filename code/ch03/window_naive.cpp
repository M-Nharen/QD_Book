// Rolling maximum of the last k prices, the obvious way: rescan the window. O(n k) (3.10).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

std::vector<int> make_prices(std::size_t n) {      // random walk in integer ticks
    static const int step[4] = {-2, -1, 1, 2};
    std::mt19937 rng(2024);
    std::vector<int> p(n);
    int px = 100'000;
    for (int& v : p) v = px += step[rng() >> 30]; // top 2 bits pick the step
    return p;
}

std::vector<int> rolling_max(const std::vector<int>& p, std::size_t k) {
    std::vector<int> out;                          // grows by push_back
    for (std::size_t i = k - 1; i < p.size(); ++i) {
        int m = p[i - k + 1];
        for (std::size_t j = i - k + 2; j <= i; ++j)   // k - 1 comparisons per output
            m = std::max(m, p[j]);
        out.push_back(m);
    }
    return out;
}

int main() {
    const std::size_t n = 2'000'000;
    const std::vector<int> p = make_prices(n);
    for (std::size_t k : {8, 64, 1024}) {
        const auto t0 = std::chrono::steady_clock::now();
        const std::vector<int> out = rolling_max(p, k);
        const auto t1 = std::chrono::steady_clock::now();
        long long sum = 0;                         // checksum, compared in Listing 3.9
        for (int v : out) sum += v;
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        std::printf("k=%5zu  checksum %lld  %8.2f ns per price\n", k, sum, ns / double(n));
    }
}
