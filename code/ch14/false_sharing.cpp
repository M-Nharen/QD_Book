// book: flags=-pthread
// False sharing (Section 14.4). Also run under tsan: every variable has one writer.
// Two threads each touch only their OWN variable; only the memory layout changes.
#include <pthread.h>
#include <sched.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr long kIters = 1'000'000;
#else
constexpr long kIters = 20'000'000;
#endif
using Counter = std::atomic<std::uint64_t>;

void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}
double now_ns() {
    return std::chrono::duration<double, std::nano>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void inc_rmw(Counter& c) { c.fetch_add(1, std::memory_order_relaxed); }      // lock add
void inc_plain(Counter& c) {                       // single writer: mov, add, mov
    c.store(c.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
}

struct Packed {                                    // a and b share one 64-byte line
    Counter a{0}, b{0};
};
struct Padded {                                    // each owns a line
    alignas(64) Counter a{0};
    alignas(64) Counter b{0};
};

// (1) Two threads, each incrementing its own counter.
template <class S, void (*Inc)(Counter&)>
double two_writers_ns() {
    double best = 1e300;
    for (int trial = 0; trial < 3; ++trial) {      // best of 3: the VM is noisy
        S s;
        const double t0 = now_ns();
        std::thread t([&] { pin(1); for (long i = 0; i < kIters; ++i) Inc(s.b); });
        pin(0);
        for (long i = 0; i < kIters; ++i) Inc(s.a);
        t.join();
        best = std::min(best, (now_ns() - t0) / double(kIters));
    }
    return best;
}

// (2) One thread increments a; the other only READS b, a value that never changes.
template <class S>
void writer_and_reader(const char* name) {
    S s;
    s.b.store(7, std::memory_order_relaxed);
    std::atomic<bool> started{false}, stop{false};
    std::thread w([&] {
        pin(1);
        started.store(true, std::memory_order_release);
        while (!stop.load(std::memory_order_relaxed)) inc_rmw(s.a);
    });
    pin(0);
    while (!started.load(std::memory_order_acquire)) { }
    const std::uint64_t a0 = s.a.load(std::memory_order_relaxed);
    std::uint64_t sum = 0;
    const double t0 = now_ns();
    for (long i = 0; i < kIters; ++i) sum += s.b.load(std::memory_order_relaxed);
    const double dt = now_ns() - t0;
    const std::uint64_t writes = s.a.load(std::memory_order_relaxed) - a0;
    stop.store(true, std::memory_order_relaxed);
    w.join();
    std::printf("  %-7s reader %5.2f ns per load;   writer %6.1f ns per increment\n",
                name, dt / double(kIters), dt / double(writes));
    if (sum != 7u * std::uint64_t(kIters)) std::printf("wrong sum\n");
}

int main() {
    std::printf("sizeof(Packed) = %zu, sizeof(Padded) = %zu\n",
                sizeof(Packed), sizeof(Padded));
    std::printf("(1) two writers, fetch_add  same line %6.2f ns   own lines %6.2f ns\n",
                two_writers_ns<Packed, inc_rmw>(), two_writers_ns<Padded, inc_rmw>());
    std::printf("    two writers, plain inc  same line %6.2f ns   own lines %6.2f ns\n",
                two_writers_ns<Packed, inc_plain>(), two_writers_ns<Padded, inc_plain>());
    std::printf("(2) one writer, one reader of a constant\n");
    writer_and_reader<Packed>("Packed");
    writer_and_reader<Padded>("Padded");
}
