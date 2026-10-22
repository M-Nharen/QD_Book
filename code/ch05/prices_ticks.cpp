// Code showdown, optimized: integer ticks end to end (Section 5.9).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Event {                                   // identical input to Listing 5.9
    std::uint8_t feed;
    std::int32_t qty;
    std::int64_t cents;
    char text[16];
};

std::vector<Event> make_events(int n) {
    std::vector<Event> ev(n);
    std::uint64_t x = 42;
    for (auto& e : ev) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        e.feed = x & 1;
        e.qty = 1 + int((x >> 8) % 100);
        e.cents = 10'000 + std::int64_t((x >> 20) % 1'000);
        std::snprintf(e.text, sizeof e.text, "%lld.%02lld",
                      static_cast<long long>(e.cents / 100),
                      static_cast<long long>(e.cents % 100));
    }
    return ev;
}

// "101.37" -> 10137. Exactly two decimals required; returns -1 on malformed input.
inline std::int64_t parse_cents(const char* s) {
    std::int64_t v = 0;
    for (; *s >= '0' && *s <= '9'; ++s) v = v * 10 + (*s - '0');
    if (s[0] != '.' || s[1] < '0' || s[1] > '9' || s[2] < '0' || s[2] > '9' || s[3])
        return -1;
    return v * 100 + (s[1] - '0') * 10 + (s[2] - '0');
}

int main() {
    const auto events = make_events(10'000'000);
    constexpr std::int64_t lo = 10'000, levels = 1'000;   // price band known up front
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<std::int64_t> volume(levels, 0); // index = ticks - lo: no tree, no hash
    std::int64_t rejected = 0;
    for (const Event& e : events) {
        const std::int64_t ticks = e.feed == 0 ? e.cents : parse_cents(e.text);
        const std::int64_t idx = ticks - lo;
        if (idx < 0 || idx >= levels) { ++rejected; continue; }  // never trust input
        volume[idx] += e.qty;
    }
    const auto t1 = std::chrono::steady_clock::now();
    std::int64_t total = 0, used = 0;
    for (std::int64_t q : volume) { total += q; used += q != 0; }
    std::printf("levels: %lld (true number of prices: 1000), rejected: %lld\n",
                static_cast<long long>(used), static_cast<long long>(rejected));
    std::printf("total quantity: %lld\n", static_cast<long long>(total));
    std::printf("volume at 100.10: %lld\n", static_cast<long long>(volume[10'010 - lo]));
    std::printf("time: %.0f ms\n",
                std::chrono::duration<double, std::milli>(t1 - t0).count());
}
