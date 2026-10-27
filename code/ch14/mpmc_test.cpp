// book: flags=-pthread
// Tests the MPMC queue of Listing 14.5 with P producers and C consumers (Section 14.6):
// every message must arrive exactly once, and each consumer must see each producer's
// messages in the order they were sent. Also run under tsan: no races reported.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
#include "mpmc_queue.hpp"

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr std::uint64_t kPer = 50'000;             // messages per producer
#else
constexpr std::uint64_t kPer = 2'000'000;
#endif

bool run(int producers, int consumers) {
    MpmcQueue<std::uint64_t> q(1024);
    const std::uint64_t total = kPer * std::uint64_t(producers);
    std::atomic<std::uint64_t> consumed{0}, sum{0};
    std::atomic<bool> in_order{true};
    const auto t0 = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> ts;
        for (std::uint64_t p = 0; p < std::uint64_t(producers); ++p)
            ts.emplace_back([&, p] {               // message = (p << 32) | sequence
                for (std::uint64_t i = 1; i <= kPer; ++i)
                    while (!q.try_push(p << 32 | i)) std::this_thread::yield();
            });
        for (int c = 0; c < consumers; ++c)
            ts.emplace_back([&] {
                std::vector<std::uint64_t> last(producers, 0);
                std::uint64_t v = 0, s = 0;
                while (consumed.load(std::memory_order_relaxed) < total) {
                    if (!q.try_pop(v)) { std::this_thread::yield(); continue; }
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    std::uint64_t& prev = last[v >> 32];
                    if ((v & 0xffff'ffff) <= prev) in_order = false;
                    prev = v & 0xffff'ffff;
                    s += v;
                }
                sum.fetch_add(s, std::memory_order_relaxed);
            });
    }                                              // jthreads join here
    const std::chrono::duration<double, std::nano> dt =
        std::chrono::steady_clock::now() - t0;
    std::uint64_t want = 0;
    for (std::uint64_t p = 0; p < std::uint64_t(producers); ++p)
        want += kPer * (p << 32) + kPer * (kPer + 1) / 2;
    const bool ok = consumed == total && sum == want && in_order;
    std::printf("  %d producer(s), %d consumer(s): %6.1f ns per message  %s\n",
                producers, consumers, dt.count() / double(total),
                ok ? "all once, in order" : "FAILED");
    return ok;
}

int main() {
    std::printf("%llu messages per producer, capacity 1024, 2 CPUs\n",
                (unsigned long long)kPer);
    bool ok = true;
    for (auto [p, c] : {std::pair{1, 1}, {2, 1}, {1, 2}, {2, 2}, {4, 4}}) ok &= run(p, c);
    return ok ? 0 : 1;
}
