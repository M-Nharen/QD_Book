// Shared workload for the risk-check showdown (Section 9.8): orders and a timing loop.
#pragma once
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Order {
    std::int64_t  px;        // price in ticks
    std::uint32_t qty;
    std::uint16_t symbol;    // instrument index
    char          side;      // 'B' or 'S'
};

// Risk limits as they would arrive from configuration. Reading them through volatile
// stops the compiler from treating them as constants (it must load them at run time).
struct Limits { std::uint32_t max_qty; std::int64_t ref_px, band, max_notional; };
inline Limits load_limits() {
    static volatile std::int64_t cfg[4] = {1'000, 10'000, 500, 9'000'000};
    return {static_cast<std::uint32_t>(cfg[0]), cfg[1], cfg[2], cfg[3]};
}

// SplitMix64: a tiny, good-enough generator for building the test data.
inline std::uint64_t splitmix(std::uint64_t& s) {
    std::uint64_t z = (s += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

inline std::vector<Order> make_orders(std::size_t n) {
    std::vector<Order> v(n);
    std::uint64_t s = 42;
    for (auto& o : v) {
        o.px = 10'000 + static_cast<std::int64_t>(splitmix(s) % 1'100) - 550;
        o.qty = 1 + static_cast<std::uint32_t>(splitmix(s) % 1'050);
        o.symbol = static_cast<std::uint16_t>(splitmix(s) % 512);
        o.side = (splitmix(s) & 1) ? 'B' : 'S';
    }
    return v;
}

// Runs check(o) over every order, `passes` times; prints ns/order and the reject count.
template <class F>
void run(const char* name, const std::vector<Order>& orders, F&& check, int passes = 30) {
    long rejected = 0;
    for (const auto& o : orders) rejected += !check(o);                  // warm-up
    const auto t0 = std::chrono::steady_clock::now();
    long sink = 0;
    for (int p = 0; p < passes; ++p)
        for (const auto& o : orders) sink += check(o);
    const auto t1 = std::chrono::steady_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    std::printf("%-34s rejected %ld of %zu  %6.2f ns/order  (checksum %ld)\n", name,
                rejected, orders.size(), ns / (double(passes) * orders.size()), sink);
}
