// book: flags=-pthread
// The cost of blocking and waking (Section 13.4). Also run under ThreadSanitizer (tsan).
// Two threads hand a token back and forth through a raw futex, sleeping in the kernel
// between turns. Run with both threads pinned to one CPU (every handoff is a context
// switch on that CPU) and on two CPUs (every handoff wakes a sleeper on the other CPU).
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

constexpr int kRounds = 20000;

static void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}
struct Sw { long vol, invol; };
static Sw switches_now() {                        // context switches of this thread
    rusage r{};
    getrusage(RUSAGE_THREAD, &r);
    return {r.ru_nvcsw, r.ru_nivcsw};
}

// ---- futex ping-pong: whose turn it is lives in one atomic int
static std::atomic<int> turn{0};
static long futex(std::atomic<int>* a, int op, int val) {
    return syscall(SYS_futex, reinterpret_cast<int*>(a), op, val, nullptr, nullptr, 0);
}
static void wait_for(int mine) {                  // sleep until turn == mine
    for (int v = turn.load(); v != mine; v = turn.load())
        futex(&turn, FUTEX_WAIT_PRIVATE, v);       // sleeps only if turn still == v
}
static void hand_to(int next) {
    turn.store(next);
    futex(&turn, FUTEX_WAKE_PRIVATE, 1);
}
static double futex_pingpong(int cpu_a, int cpu_b, Sw& sw) {
    turn = 0;
    std::thread peer([&] {
        pin(cpu_b);
        for (int i = 0; i < kRounds; ++i) { wait_for(1); hand_to(0); }
    });
    pin(cpu_a);
    const Sw s0 = switches_now();
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kRounds; ++i) { hand_to(1); wait_for(0); }
    const auto t1 = std::chrono::steady_clock::now();
    const Sw s1 = switches_now();
    sw = {s1.vol - s0.vol, s1.invol - s0.invol};
    peer.join();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / (2.0 * kRounds);
}

int main() {
    const int ncpu = static_cast<int>(std::thread::hardware_concurrency());
    const int other = ncpu > 1 ? 1 : 0;
    Sw sw{};
    std::printf("%d rounds; ns per one-way handoff; switches of the timing thread\n",
                kRounds);
    auto show = [&](const char* what, double ns) {
        std::printf("%-9s %7.0f ns   voluntary %6ld  involuntary %6ld\n", what, ns,
                    sw.vol, sw.invol);
    };
    show("same CPU", futex_pingpong(0, 0, sw));
    show("two CPUs", futex_pingpong(0, other, sw));

    pin(0);                                        // a syscall that does not switch
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kRounds; ++i) sched_yield();
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("sched_yield, nothing else runnable: %.0f ns\n",
                std::chrono::duration<double, std::nano>(t1 - t0).count() / kRounds);
}
