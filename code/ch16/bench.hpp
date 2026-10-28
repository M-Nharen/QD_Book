// A minimal benchmarking toolkit (Section 16.4): keep results alive, fence the timestamp
// counter, convert ticks to nanoseconds, pin the thread, and summarize a distribution.
#pragma once
#include <pthread.h>
#include <sched.h>
#include <x86intrin.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace bench {

// The compiler must assume the asm reads `v` (in a register or in memory) and may read or
// write any memory, so it can neither delete the computation of `v` nor move it away.
template <class T>
inline void do_not_optimize(const T& v) { asm volatile("" : : "r,m"(v) : "memory"); }

// lfence: earlier instructions finish before rdtsc reads the counter. rdtscp waits for
// earlier instructions itself; the lfence after it stops later ones starting early.
inline std::uint64_t start() { _mm_lfence(); return __rdtsc(); }
inline std::uint64_t stop() {
    unsigned aux;
    const std::uint64_t t = __rdtscp(&aux);
    _mm_lfence();
    return t;
}

inline double ticks_per_ns() {                         // calibrate the TSC once (13.5)
    const auto c0 = std::chrono::steady_clock::now();
    const std::uint64_t t0 = start();
    while (std::chrono::steady_clock::now() - c0 < std::chrono::milliseconds(50)) {}
    const std::uint64_t t1 = stop();
    const auto c1 = std::chrono::steady_clock::now();
    return double(t1 - t0) / double(std::chrono::nanoseconds(c1 - c0).count());
}

inline void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}

struct Summary { double mean, p50, p99, p999, max; };

inline Summary summarize(std::vector<double> v) {      // p-th: sorted[round(p(n-1))]
    std::sort(v.begin(), v.end());
    auto at = [&](double p) { return v[std::size_t(p * double(v.size() - 1) + 0.5)]; };
    double sum = 0;
    for (double x : v) sum += x;
    return {sum / double(v.size()), at(0.50), at(0.99), at(0.999), v.back()};
}

}  // namespace bench
