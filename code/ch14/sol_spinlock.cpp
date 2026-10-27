// book: flags=-pthread
// Solution to P14.6: a test-and-test-and-set spinlock with pause and bounded
// exponential backoff, compared with a naive exchange loop and std::mutex. tsan-clean.
#include <pthread.h>
#include <sched.h>
#include <immintrin.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr long kIters = 100'000;
#else
constexpr long kIters = 4'000'000;
#endif

class NaiveSpinlock {                         // every spin is a locked RMW
    std::atomic<bool> locked_{false};
public:
    void lock() { while (locked_.exchange(true, std::memory_order_acquire)) { } }
    void unlock() { locked_.store(false, std::memory_order_release); }
};

class Spinlock {
    alignas(64) std::atomic<bool> locked_{false};
public:
    void lock() {
        unsigned backoff = 1;
        for (;;) {
            if (!locked_.exchange(true, std::memory_order_acquire)) return;  // got it
            do {                                   // wait with plain loads: line stays S
                for (unsigned i = 0; i < backoff; ++i) _mm_pause();
                backoff = std::min(backoff * 2, 64u);
            } while (locked_.load(std::memory_order_relaxed));
        }
    }
    bool try_lock() {
        return !locked_.load(std::memory_order_relaxed) &&
               !locked_.exchange(true, std::memory_order_acquire);
    }
    void unlock() { locked_.store(false, std::memory_order_release); }
};

void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}

template <class L>
double ns_per_section(const char* name) {
    L lk;
    long counter = 0;                              // plain: protected by the lock
    auto work = [&](int cpu) {
        pin(cpu);
        for (long i = 0; i < kIters; ++i) {
            std::lock_guard g(lk);
            ++counter;
        }
    };
    const auto t0 = std::chrono::steady_clock::now();
    std::thread t(work, 1);
    work(0);
    t.join();
    const double ns = std::chrono::duration<double, std::nano>(
                          std::chrono::steady_clock::now() - t0).count() / (2.0 * kIters);
    std::printf("  %-26s %6.1f ns per critical section, count %s\n", name, ns,
                counter == 2 * kIters ? "correct" : "WRONG");
    return ns;
}

int main() {
    std::printf("two threads, %ld lock/increment/unlock each\n", kIters);
    ns_per_section<NaiveSpinlock>("naive exchange loop");
    ns_per_section<Spinlock>("TTAS + pause + backoff");
    ns_per_section<std::mutex>("std::mutex");
    Spinlock s;
    s.lock();
    const bool second = s.try_lock();
    s.unlock();
    std::printf("try_lock on a held lock returns %s\n", second ? "true (BUG)" : "false");
}
