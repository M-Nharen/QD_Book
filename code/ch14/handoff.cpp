// book: flags=-pthread
// Spinning vs blocking (Section 14.9): two threads pass a turn back and forth, waiting
// in three different ways. Reports one-way latency and CPU time burnt. tsan-clean.
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <immintrin.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr int kRounds = 2'000;
#else
constexpr int kRounds = 50'000;
#endif

void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}
double cpu_seconds() {                             // user + system time, whole process
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return double(r.ru_utime.tv_sec + r.ru_stime.tv_sec) +
           1e-6 * double(r.ru_utime.tv_usec + r.ru_stime.tv_usec);
}

// Each waiter type offers: wait_for(v) blocks until turn == v; set(v) makes turn = v.
struct CondVar {
    std::mutex m;
    std::condition_variable cv;
    int turn = 0;                                  // guarded by m
    void wait_for(int v) {
        std::unique_lock lk(m);
        cv.wait(lk, [&] { return turn == v; });
    }
    void set(int v) {
        { std::lock_guard lk(m); turn = v; }
        cv.notify_one();
    }
};
struct AtomicWait {                                // C++20 atomic wait/notify
    std::atomic<int> turn{0};
    void wait_for(int v) {
        for (int t = turn.load(std::memory_order_acquire); t != v;
             t = turn.load(std::memory_order_acquire))
            turn.wait(t, std::memory_order_acquire);   // sleeps while turn == t
    }
    void set(int v) { turn.store(v, std::memory_order_release); turn.notify_one(); }
};
struct Spin {
    std::atomic<int> turn{0};
    void wait_for(int v) {
        while (turn.load(std::memory_order_acquire) != v) _mm_pause();
    }
    void set(int v) { turn.store(v, std::memory_order_release); }
};
template <class W>
void measure(const char* name) {
    W w;
    std::thread peer([&] {
        pin(1);
        for (int i = 0; i < kRounds; ++i) { w.wait_for(1); w.set(0); }
    });
    pin(0);
    const double c0 = cpu_seconds();
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kRounds; ++i) { w.set(1); w.wait_for(0); }
    const auto t1 = std::chrono::steady_clock::now();
    peer.join();
    const double wall = std::chrono::duration<double>(t1 - t0).count();
    const double cpu = cpu_seconds() - c0;
    std::printf("  %-22s %8.0f ns one way   CPU busy %4.0f%% of 2 CPUs\n",
                name, wall * 1e9 / (2.0 * kRounds), 100.0 * cpu / (2.0 * wall));
}

int main() {
    std::printf("%d round trips, threads pinned to CPUs 0 and 1\n", kRounds);
    measure<CondVar>("mutex + condvar");
    measure<AtomicWait>("atomic wait/notify");
    measure<Spin>("spin with pause");
}
