// Code showdown (Section 7.8): the same message handler with general-purpose heap
// allocation versus a pre-faulted object pool plus a per-message arena.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <utility>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <x86intrin.h>

struct Order {                                     // 64 bytes: one cache line if aligned
    std::uint64_t id;
    std::int64_t price;
    std::uint32_t qty, filled;
    std::uint64_t ts_in, ts_ack;
    char side, tif, pad[22];
};
static_assert(sizeof(Order) == 64);

Order make_order(std::uint64_t id) {
    Order o{};                                     // value-initialize every field
    o.id = id; o.price = 9950; o.qty = 10; o.ts_in = id; o.side = 'B';
    return o;
}

struct Rng {                                       // xoshiro256** (Listing 2.2), condensed
    std::uint64_t s[4] = {0x9e3779b97f4a7c15, 0xbf58476d1ce4e5b9, 0x94d049bb133111eb, 7};
    static std::uint64_t rotl(std::uint64_t v, int k) { return (v << k) | (v >> (64 - k)); }
    std::uint64_t next() {
        const std::uint64_t r = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t;    s[3] = rotl(s[3], 45);
        return r;
    }
    std::uint64_t below(std::uint64_t n) {
        return std::uint64_t((static_cast<unsigned __int128>(next()) * n) >> 64);
    }
};

constexpr std::size_t kLive = 50'000;              // orders resting at any time

// ---------------------------------------------------------------- naive: the heap
struct NaiveHandler {
    std::vector<std::unique_ptr<Order>> live;
    NaiveHandler() { live.reserve(2 * kLive); }
    std::uint64_t on_message(Rng& rng, std::uint64_t id) {
        std::vector<char> scratch(32 + rng.below(993));    // decode buffer: malloc
        std::memset(scratch.data(), int(id), scratch.size());
        std::uint64_t sum = std::uint64_t(scratch[scratch.size() / 2]);
        if (live.size() < kLive || rng.below(2) == 0) {    // add: one new per order
            live.push_back(std::make_unique<Order>(make_order(id)));
        } else {                                           // cancel: one delete
            const std::size_t i = rng.below(live.size());
            std::swap(live[i], live.back());
            live.pop_back();
        }
        Order& o = *live[rng.below(live.size())];          // a fill touches some order
        o.filled += 1;
        return sum + o.qty;
    }                                                      // scratch freed here: free()
};

// ---------------------------------------------------------------- optimized
template <class T> class ObjectPool {
    union Slot {                                   // a free slot stores the next pointer;
        Slot* next;                                // a used slot stores a T
        alignas(T) std::byte storage[sizeof(T)];
    };
    struct alignas(64) Line { Slot s; };           // 64-byte stride: one line per object
    std::unique_ptr<Line[]> slab_;
    Slot* free_ = nullptr;

public:
    explicit ObjectPool(std::size_t capacity) : slab_(new Line[capacity]) {
        for (std::size_t i = capacity; i-- > 0;) { // writing every slot also pre-faults
            slab_[i].s.next = free_;               // every page of the slab
            free_ = &slab_[i].s;
        }
    }
    template <class... A> T* create(A&&... a) {
        if (free_ == nullptr) return nullptr;      // fixed capacity: caller handles it
        Slot* s = free_;
        free_ = s->next;
        return ::new (static_cast<void*>(s->storage)) T{std::forward<A>(a)...};
    }
    void destroy(T* p) noexcept {
        p->~T();
        Slot* s = reinterpret_cast<Slot*>(p);      // storage is at offset 0 of the slot
        s->next = free_;
        free_ = s;
    }
};

class ScratchArena {                               // Listing 7.5's idea, minimal form
    std::unique_ptr<char[]> buf_;
    std::size_t cap_, used_ = 0;
public:
    explicit ScratchArena(std::size_t cap) : buf_(new char[cap]), cap_(cap) {
        std::memset(buf_.get(), 0, cap_);          // pre-fault
    }
    char* take(std::size_t n) {
        if (used_ + n > cap_) return nullptr;
        char* p = buf_.get() + used_;
        used_ += n;
        return p;
    }
    void reset() { used_ = 0; }
};

struct PooledHandler {
    ObjectPool<Order> pool{2 * kLive};
    ScratchArena arena{64 * 1024};
    std::vector<Order*> live;
    PooledHandler() { live.reserve(2 * kLive); }
    ~PooledHandler() { for (Order* o : live) pool.destroy(o); }
    std::uint64_t on_message(Rng& rng, std::uint64_t id) {
        const std::size_t len = 32 + rng.below(993);
        char* scratch = arena.take(len);                   // bump pointer, no malloc
        std::memset(scratch, int(id), len);
        std::uint64_t sum = std::uint64_t(scratch[len / 2]);
        if (live.size() < kLive || rng.below(2) == 0) {
            live.push_back(pool.create(make_order(id)));
        } else {
            const std::size_t i = rng.below(live.size());
            std::swap(live[i], live.back());
            pool.destroy(live.back());
            live.pop_back();
        }
        Order& o = *live[rng.below(live.size())];
        o.filled += 1;
        arena.reset();                                     // end of message
        return sum + o.qty;
    }
};

// ---------------------------------------------------------------- measurement
double tsc_per_ns() {                              // calibrate the TSC (Section 16.4)
    const auto t0 = std::chrono::steady_clock::now();
    const std::uint64_t c0 = __rdtsc();
    while (std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(200)) {}
    const std::uint64_t c1 = __rdtsc();
    const auto t1 = std::chrono::steady_clock::now();
    return double(c1 - c0) / std::chrono::duration<double, std::nano>(t1 - t0).count();
}

template <class Handler> void run(const char* name, double ghz) {
    constexpr int kWarm = 200'000, kMeasured = 2'000'000;
    const auto c0 = std::chrono::steady_clock::now();
    auto h = std::make_unique<Handler>();
    const double setup_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c0)
            .count();
    Rng rng;
    std::uint64_t sink = 0, id = 0;
    for (int i = 0; i < kWarm; ++i) sink += h->on_message(rng, ++id);
    std::vector<std::uint32_t> cycles(kMeasured);
    for (int i = 0; i < kMeasured; ++i) {
        const std::uint64_t t0 = __rdtsc();
        sink += h->on_message(rng, ++id);
        cycles[i] = std::uint32_t(std::min<std::uint64_t>(__rdtsc() - t0, UINT32_MAX));
    }
    std::sort(cycles.begin(), cycles.end());
    auto pct = [&](double p) { return cycles[std::size_t(p * (kMeasured - 1))] / ghz; };
    double mean = 0;
    for (auto c : cycles) mean += c;
    mean /= kMeasured * ghz;
    std::printf("%-7s setup %6.1f ms | ns/msg mean %5.1f  p50 %5.1f  p99 %6.1f  "
                "p99.9 %6.1f  (sink %llu)\n", name, setup_ms, mean, pct(0.5), pct(0.99),
                pct(0.999), static_cast<unsigned long long>(sink % 10));
}

void isolated(void (*fn)(const char*, double), const char* name, double ghz) {
    std::fflush(stdout);
    if (const pid_t pid = fork(); pid == 0) {
        fn(name, ghz);
        std::fflush(stdout);
        _exit(0);
    } else {
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

int main() {
    const double ghz = tsc_per_ns();
    std::printf("TSC: %.2f ticks/ns. 2M messages after 200k warm-up, %zu live orders.\n",
                ghz, kLive);
    for (int rep = 0; rep < 2; ++rep) {            // each run in a fresh process, so
        isolated(run<NaiveHandler>, "naive", ghz); // no run inherits another's heap
        isolated(run<PooledHandler>, "pooled", ghz);
    }
}
