// book: flags=-pthread
// The cost of synchronization instructions on one thread, on a line this core already
// owns, and of one mutex shared by two threads (Sections 14.1, 14.11). tsan-clean.
#include <immintrin.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr long kN = 200'000;
#else
constexpr long kN = 20'000'000;
#endif

std::atomic<std::uint64_t> a{0};
std::mutex m;

long count = 0, handoffs = 0;                      // guarded by m
int last_owner = -1;                               // guarded by m
void hammer(int me, long n) {                      // lock, increment, unlock, n times
    for (long i = 0; i < n; ++i) {
        std::lock_guard lk(m);
        ++count;
        if (last_owner != me) { last_owner = me; ++handoffs; }
    }
}

template <class F>
double ns_per_op(F f) {
    double best = 1e300;
    for (int trial = 0; trial < 5; ++trial) {
        const auto t0 = std::chrono::steady_clock::now();
        for (long i = 0; i < kN; ++i) f(i);
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kN;
        if (ns < best) best = ns;
    }
    return best;
}

int main() {
    using std::memory_order_relaxed, std::memory_order_release, std::memory_order_seq_cst;
    std::uint64_t sink = 0;
    // glibc skips the atomic in pthread_mutex_lock while the process has one thread.
    const double mutex_single = ns_per_op([](long) { m.lock(); m.unlock(); });
    std::thread([] {}).join();                     // now the process is multi-threaded
    const struct { const char* name; double ns; } rows[] = {
        {"load (any order): mov", ns_per_op([&](long) { sink += a.load(); })},
        {"store relaxed/release: mov",
         ns_per_op([](long i) { a.store(std::uint64_t(i), memory_order_release); })},
        {"store seq_cst: xchg",
         ns_per_op([](long i) { a.store(std::uint64_t(i), memory_order_seq_cst); })},
        {"fetch_add, result unused: lock add",
         ns_per_op([](long) { a.fetch_add(1, memory_order_relaxed); })},
        {"compare_exchange (succeeds): lock cmpxchg", ns_per_op([](long i) {
             std::uint64_t e = a.load(memory_order_relaxed);
             a.compare_exchange_strong(e, std::uint64_t(i), memory_order_relaxed);
         })},
        {"atomic_thread_fence(seq_cst): lock or",
         ns_per_op([](long) { std::atomic_thread_fence(memory_order_seq_cst); })},
        {"_mm_mfence()", ns_per_op([](long) { _mm_mfence(); })},
        {"_mm_pause()", ns_per_op([](long) { _mm_pause(); })},
        {"std::mutex lock + unlock, before any thread", mutex_single},
        {"std::mutex lock + unlock, after one thread",
         ns_per_op([](long) { m.lock(); m.unlock(); })},
    };
    std::printf("%-46s %7s\n", "operation (uncontended, one thread)", "ns/op");
    for (const auto& r : rows) std::printf("%-46s %7.2f\n", r.name, r.ns);
    if (sink == 1) std::printf("(unlikely)\n");    // keeps the loads alive

    const long n = kN / 5;                         // two threads, one mutex
    const auto t0 = std::chrono::steady_clock::now();
    {
        std::jthread other(hammer, 1, n);
        hammer(0, n);
    }
    const std::chrono::duration<double, std::nano> dt =
        std::chrono::steady_clock::now() - t0;
    std::printf("two threads, one mutex: %.1f ns per increment, count %s\n",
                dt.count() / double(2 * n), count == 2 * n ? "correct" : "WRONG");
    std::printf("  ownership changed hands %ld times in %ld increments\n", handoffs, 2 * n);
}
