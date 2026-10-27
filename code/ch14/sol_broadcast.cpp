// book: flags=-pthread
// Solution to P14.11: a single-producer, multi-consumer BROADCAST ring. The writer never
// waits; every reader sees every message unless it falls a full lap behind, in which
// case it detects the overrun, counts the loss and resynchronizes. tsan-clean.
#include <pthread.h>
#include <immintrin.h>
#include <sched.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr std::uint64_t kMsgs = 200'000;
#else
constexpr std::uint64_t kMsgs = 10'000'000;
#endif

struct Msg { std::uint64_t n, a, b, check; };      // check = n ^ a ^ b

template <std::size_t N>                           // N: a power of two
class BroadcastRing {
    static constexpr std::size_t W = sizeof(Msg) / 8;
    struct alignas(64) Slot {
        std::atomic<std::uint64_t> seq{0};         // 2n+1: writing message n; 2n+2: done
        std::atomic<std::uint64_t> w[W] = {};
    };
    Slot slots_[N];
    alignas(64) std::atomic<std::uint64_t> next_{0};   // number of messages published

public:
    void publish(const Msg& m) {                   // single writer; never blocks
        const std::uint64_t n = next_.load(std::memory_order_relaxed);
        Slot& s = slots_[n & (N - 1)];
        std::uint64_t buf[W];
        std::memcpy(buf, &m, sizeof m);
        s.seq.store(2 * n + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t i = 0; i < W; ++i) s.w[i].store(buf[i], std::memory_order_relaxed);
        s.seq.store(2 * n + 2, std::memory_order_release);
        next_.store(n + 1, std::memory_order_release);
    }

    enum class Got { Message, Empty, Overrun };
    Got read(std::uint64_t n, Msg& out) const {    // try to read message number n
        const Slot& s = slots_[n & (N - 1)];
        const std::uint64_t s1 = s.seq.load(std::memory_order_acquire);
        if (s1 < 2 * n + 2) return Got::Empty;     // not written yet (or in progress)
        if (s1 > 2 * n + 2) return Got::Overrun;   // the slot already holds a later lap
        std::uint64_t buf[W];
        for (std::size_t i = 0; i < W; ++i) buf[i] = s.w[i].load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (s.seq.load(std::memory_order_relaxed) != s1) return Got::Overrun;  // lapped
        std::memcpy(&out, buf, sizeof out);
        return Got::Message;
    }
    std::uint64_t published() const { return next_.load(std::memory_order_acquire); }
};

void pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof s, &s);
}

struct Stats { std::uint64_t received = 0, lost = 0, bad = 0, overruns = 0; };

int main() {
    auto* ring = new BroadcastRing<1024>;
    std::vector<Stats> stats(2);
    auto reader = [&](int id, int cpu) {
        pin(cpu);
        Stats& st = stats[std::size_t(id)];
        std::uint64_t n = 0;
        Msg m{};
        while (n < kMsgs) {
            switch (ring->read(n, m)) {
            case BroadcastRing<1024>::Got::Message:
                st.bad += (m.n != n || m.check != (m.n ^ m.a ^ m.b));
                ++st.received;
                ++n;
                break;
            case BroadcastRing<1024>::Got::Empty:
                if (cpu == 0) std::this_thread::yield();   // let the writer on our CPU run
                else _mm_pause();
                break;
            case BroadcastRing<1024>::Got::Overrun: {
                const std::uint64_t next = ring->published() - 1024 / 2;  // half a lap
                st.lost += next - n;                                      // back: still
                ++st.overruns;                                            // in the ring
                n = next;
                break;
            }
            }
        }
    };
    std::thread r0(reader, 0, 1);                 // reader 0 has a CPU to itself
    std::thread r1(reader, 1, 0);                 // reader 1 shares the writer's CPU
    pin(0);
    for (std::uint64_t n = 0; n < kMsgs; ++n)
        ring->publish(Msg{n, 3 * n, n * n, n ^ (3 * n) ^ (n * n)});
    r0.join();
    r1.join();
    bool ok = true;
    for (int i = 0; i < 2; ++i) {
        const Stats& st = stats[std::size_t(i)];
        std::printf("reader %d: received %llu, lost %llu in %llu overruns, %llu bad\n", i,
                    (unsigned long long)st.received, (unsigned long long)st.lost,
                    (unsigned long long)st.overruns, (unsigned long long)st.bad);
        ok = ok && st.bad == 0 && st.received + st.lost == kMsgs;
    }
    delete ring;
    std::printf("%s\n", ok ? "every message received once or counted as lost" : "FAILED");
    return ok ? 0 : 1;
}
