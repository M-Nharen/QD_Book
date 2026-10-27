// book: flags=-pthread
// The store-buffering litmus test on real hardware (Sections 14.2, 14.3); tsan-clean.
// Thread A: X = 1; r1 = Y.     Thread B: Y = 1; r2 = X.     Can both r1 and r2 be 0?
#include <pthread.h>
#include <sched.h>
#include <immintrin.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>
using std::memory_order;

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr int kIters = 20'000;
#else
constexpr int kIters = 200'000;
#endif

struct alignas(64) Line { std::atomic<int> v{0}; };
Line X, Y;                                         // separate lines: no false sharing

class SpinBarrier {                                // two threads, sense-reversing
    alignas(64) std::atomic<int> count_{0};
    alignas(64) std::atomic<int> sense_{0};
public:
    void arrive(int& local_sense) {
        local_sense ^= 1;
        if (count_.fetch_add(1, std::memory_order_acq_rel) == 1) {
            count_.store(0, std::memory_order_relaxed);
            sense_.store(local_sense, std::memory_order_release);
        } else {
            while (sense_.load(std::memory_order_acquire) != local_sense) { }
        }
    }
};

void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}

// One side of the test: store 1 to `mine`, optionally fence, then load `other`.
template <memory_order St, memory_order Ld, bool Fence>
int side(Line& mine, Line& other) {
    mine.v.store(1, St);
    if constexpr (Fence) std::atomic_thread_fence(std::memory_order_seq_cst);
    return other.v.load(Ld);
}

template <memory_order St, memory_order Ld, bool Fence = false>
long count_both_zero() {
    SpinBarrier bar;
    std::vector<int> r1(kIters), r2(kIters);       // each written by one thread only
    auto run = [&](int cpu, Line& mine, Line& other, std::vector<int>& r) {
        pin(cpu);
        int sense = 0;
        std::uint32_t rng = 12345u + unsigned(cpu);
        for (int i = 0; i < kIters; ++i) {
            bar.arrive(sense);                     // both threads start together...
            rng = rng * 1664525u + 1013904223u;
            for (unsigned d = rng >> 29; d != 0; --d) _mm_pause();  // ...jittered
            r[i] = side<St, Ld, Fence>(mine, other);
            bar.arrive(sense);                     // both finished this round
            if (cpu == 0) {
                X.v.store(0, std::memory_order_relaxed);
                Y.v.store(0, std::memory_order_relaxed);
            }
            bar.arrive(sense);                     // reset is visible before next round
        }
    };
    std::thread b(run, 1, std::ref(Y), std::ref(X), std::ref(r2));
    run(0, X, Y, r1);
    b.join();
    long both = 0;
    for (int i = 0; i < kIters; ++i) both += (r1[i] == 0 && r2[i] == 0);
    return both;
}

int main() {
    using enum std::memory_order;
    std::printf("%d rounds per row; r1 == r2 == 0 observed:\n", kIters);
    const long rlx = count_both_zero<relaxed, relaxed>();
    const long rel = count_both_zero<release, acquire>();
    std::printf("  relaxed store, relaxed load %6ld\n", rlx);
    std::printf("  release store, acquire load %6ld\n", rel);
    const long sc = count_both_zero<seq_cst, seq_cst>();
    const long fence = count_both_zero<relaxed, relaxed, true>();
    std::printf("  seq_cst store, seq_cst load %6ld\n", sc);
    std::printf("  relaxed + seq_cst fence     %6ld\n", fence);
    return sc == 0 && fence == 0 ? 0 : 1;          // these two must never happen
}
