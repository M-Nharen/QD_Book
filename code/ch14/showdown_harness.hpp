// Shared harness for the Chapter 14 showdown (Section 14.10): a feed thread sends
// Quotes to a strategy thread; we measure throughput and paced one-way latency.
#pragma once
#include <pthread.h>
#include <sched.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr std::uint64_t kBurst = 50'000, kPaced = 2'000;
#else
constexpr std::uint64_t kBurst = 2'000'000, kPaced = 50'000;
#endif
constexpr std::int64_t kGapNs = 10'000;           // paced phase: one message per 10 us

struct Quote {                                    // 32 bytes
    std::uint64_t seq;
    std::int64_t sent_ns;
    std::int32_t bid, ask, bid_qty, ask_qty;
};

inline std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}

// send(const Quote&) runs on CPU 0, recv() -> Quote on CPU 1.
template <class Send, class Recv>
void run_showdown(Send send, Recv recv) {
    std::vector<std::int64_t> lat(kPaced);        // touched before the run (7.4)
    std::int64_t burst_end = 0;
    long bad = 0;
    std::thread strategy([&] {
        pin(1);
        std::uint64_t expect = 0;
        for (std::uint64_t i = 0; i < kBurst; ++i) bad += (recv().seq != expect++);
        burst_end = now_ns();
        for (std::uint64_t i = 0; i < kPaced; ++i) {
            const Quote q = recv();
            lat[i] = now_ns() - q.sent_ns;
            bad += (q.seq != expect++);
        }
    });
    pin(0);
    std::uint64_t seq = 0;
    const std::int64_t t0 = now_ns();
    for (std::uint64_t i = 0; i < kBurst; ++i) send(Quote{seq++, 0, 100, 101, 5, 7});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));   // let it drain
    for (std::uint64_t i = 0; i < kPaced; ++i) {
        const std::int64_t due = now_ns() + kGapNs;
        while (now_ns() < due) { }                // pace without sleeping
        send(Quote{seq++, now_ns(), 100, 101, 5, 7});
    }
    strategy.join();
    std::sort(lat.begin(), lat.end());
    auto pct = [&](double p) { return lat[std::size_t(p * double(kPaced - 1))]; };
    std::printf("throughput: %llu messages in %.1f ms = %.1f ns per message\n",
                (unsigned long long)kBurst, double(burst_end - t0) / 1e6,
                double(burst_end - t0) / double(kBurst));
    std::printf("paced latency, ns:  p50 %lld  p90 %lld  p99 %lld  p99.9 %lld  max %lld\n",
                (long long)pct(0.5), (long long)pct(0.9), (long long)pct(0.99),
                (long long)pct(0.999), (long long)lat.back());
    if (bad) std::printf("SEQUENCE ERRORS: %ld\n", bad);
}
