// Code showdown, naive: volume per price level with double prices (Section 5.9).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

struct Event {                                   // one trade from one of two feeds
    std::uint8_t feed;                           // 0: binary feed, price in cents
    std::int32_t qty;                            // 1: text feed, price as "101.37"
    std::int64_t cents;
    char text[16];
};

std::vector<Event> make_events(int n) {          // deterministic synthetic input
    std::vector<Event> ev(n);
    std::uint64_t x = 42;
    for (auto& e : ev) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17; // xorshift64: fine for test data
        e.feed = x & 1;
        e.qty = 1 + int((x >> 8) % 100);
        e.cents = 10'000 + std::int64_t((x >> 20) % 1'000);    // 100.00 .. 109.99
        std::snprintf(e.text, sizeof e.text, "%lld.%02lld",
                      static_cast<long long>(e.cents / 100),
                      static_cast<long long>(e.cents % 100));
    }
    return ev;
}

int main() {
    const auto events = make_events(10'000'000);
    const auto t0 = std::chrono::steady_clock::now();
    std::map<double, std::int64_t> volume;       // price level -> traded quantity
    for (const Event& e : events) {
        const double px = e.feed == 0 ? e.cents * 0.01               // binary path
                                      : std::strtod(e.text, nullptr); // text path
        volume[px] += e.qty;
    }
    const auto t1 = std::chrono::steady_clock::now();
    std::int64_t total = 0;
    for (const auto& [px, q] : volume) total += q;
    std::printf("levels: %zu (true number of prices: 1000)\n", volume.size());
    std::printf("total quantity: %lld\n", static_cast<long long>(total));
    const auto it = volume.find(100.10);
    std::printf("volume at 100.10: %lld\n",
                it == volume.end() ? 0LL : static_cast<long long>(it->second));
    std::printf("time: %.0f ms\n",
                std::chrono::duration<double, std::milli>(t1 - t0).count());
}
