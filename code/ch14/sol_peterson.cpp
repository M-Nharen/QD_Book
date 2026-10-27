// book: flags=-pthread
// Solution to P14.5: Peterson's lock with release/acquire vs seq_cst. tsan-clean
// (the "counter" is a relaxed atomic, so a broken lock loses updates without any UB).
#include <pthread.h>
#include <sched.h>
#include <atomic>
#include <cstdio>
#include <thread>

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr long kIters = 100'000;
#else
constexpr long kIters = 2'000'000;
#endif

void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}

template <std::memory_order St, std::memory_order Ld>
class Peterson {
    alignas(64) std::atomic<bool> flag_[2] = {false, false};
    alignas(64) std::atomic<int> victim_{0};
public:
    void lock(int me) {
        const int other = 1 - me;
        flag_[me].store(true, St);                     // I want in
        victim_.store(me, St);                         // you go first
        while (flag_[other].load(Ld) && victim_.load(Ld) == me) { }
    }
    void unlock(int me) { flag_[me].store(false, std::memory_order_release); }
};

template <class Lock>
long lost_updates() {
    Lock lk;
    std::atomic<long> counter{0};
    auto work = [&](int me) {
        pin(me);
        for (long i = 0; i < kIters; ++i) {
            lk.lock(me);
            counter.store(counter.load(std::memory_order_relaxed) + 1,   // not an RMW:
                          std::memory_order_relaxed);                    // overlap loses
            lk.unlock(me);
        }
    };
    std::thread t(work, 1);
    work(0);
    t.join();
    return 2 * kIters - counter.load();
}

int main() {
    using std::memory_order_release, std::memory_order_acquire, std::memory_order_seq_cst;
    std::printf("lost updates out of %ld:\n", 2 * kIters);
    std::printf("  release stores, acquire loads: %ld\n",
                lost_updates<Peterson<memory_order_release, memory_order_acquire>>());
    const long sc = lost_updates<Peterson<memory_order_seq_cst, memory_order_seq_cst>>();
    std::printf("  seq_cst stores and loads:      %ld\n", sc);
    return sc == 0 ? 0 : 1;
}
