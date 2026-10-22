// book: flags=-pthread
// Cache coherence costs (Section 12.3). Also run under ThreadSanitizer (tsan): no races.
// (a) one thread increments an atomic that only it uses; (b) two threads increment their
// own atomics on separate cache lines; (c) two threads increment the SAME atomic, so its
// line moves between the cores' caches; (d) two threads hand a flag back and forth.
#include <pthread.h>
#include <sched.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

struct alignas(64) Counter { std::atomic<std::uint64_t> v{0}; };   // one per cache line

void pin_to_cpu(int cpu) {                    // best effort; see 13.6 for CPU pinning
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof set, &set);
}

double now_ns() {
    return std::chrono::duration<double, std::nano>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

double two_threads_ns(Counter& c0, Counter& c1, std::uint64_t iters) {
    auto work = [iters](Counter& c, int cpu) {
        pin_to_cpu(cpu);
        for (std::uint64_t i = 0; i < iters; ++i)
            c.v.fetch_add(1, std::memory_order_relaxed);
    };
    const double t0 = now_ns();
    std::thread a(work, std::ref(c0), 0), b(work, std::ref(c1), 1);
    a.join();
    b.join();
    return (now_ns() - t0) / double(iters);   // wall time per increment, per thread
}

double ping_pong_ns(int rounds) {             // (d): one cache line handed back and forth
    std::atomic<int> turn{0};
    std::thread other([&] {
        pin_to_cpu(1);
        for (int r = 0; r < rounds; ++r) {
            while (turn.load(std::memory_order_acquire) != 1) { }
            turn.store(0, std::memory_order_release);
        }
    });
    const double t0 = now_ns();
    for (int r = 0; r < rounds; ++r) {
        turn.store(1, std::memory_order_release);
        while (turn.load(std::memory_order_acquire) != 0) { }
    }
    const double rt = (now_ns() - t0) / rounds;
    other.join();
    return rt;
}

int main() {
    const std::uint64_t iters = 2'000'000;
    Counter c0, c1;

    pin_to_cpu(0);
    double t0 = now_ns();
    for (std::uint64_t i = 0; i < iters; ++i) c0.v.fetch_add(1, std::memory_order_relaxed);
    std::printf("(a) uncontended atomic increment     %6.1f ns\n", (now_ns() - t0) / iters);

    double b = 1e300, c = 1e300, d = 1e300;
    for (int trial = 0; trial < 5; ++trial) {       // best of 5: the VM's vCPUs are noisy
        b = std::min(b, two_threads_ns(c0, c1, iters));
        c = std::min(c, two_threads_ns(c0, c0, iters));
        d = std::min(d, ping_pong_ns(100'000));
    }
    std::printf("(b) two threads, separate lines      %6.1f ns per increment\n", b);
    std::printf("(c) two threads, same atomic         %6.1f ns per increment\n", c);
    std::printf("(d) ping-pong round trip             %6.1f ns (one way about %.1f ns)\n",
                d, d / 2);
    std::printf("hardware threads available: %u\n", std::thread::hardware_concurrency());
}
