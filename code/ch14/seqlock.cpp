// book: flags=-pthread
// A seqlock: one writer, any number of readers, readers never write (Section 14.7).
// The protected data is held in relaxed atomic words, so there is no data race even
// while a reader overlaps a write; run under tsan too (it warns that it ignores fences).
#include <immintrin.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <type_traits>

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
constexpr long kReads = 200'000;
#else
constexpr long kReads = 20'000'000;
#endif

template <class T>
class Seqlock {
    static_assert(std::is_trivially_copyable_v<T> && sizeof(T) % 8 == 0);
    static constexpr std::size_t W = sizeof(T) / 8;
    alignas(64) std::atomic<std::uint64_t> seq_{0};    // odd while a write is in progress
    std::atomic<std::uint64_t> words_[W] = {};

public:
    void store(const T& v) {                            // ONE writer thread only
        std::uint64_t buf[W];
        std::memcpy(buf, &v, sizeof(T));
        const std::uint64_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);   // odd: write in progress
        std::atomic_thread_fence(std::memory_order_release);  // odd seq before any data
        for (std::size_t i = 0; i < W; ++i)
            words_[i].store(buf[i], std::memory_order_relaxed);
        seq_.store(s + 2, std::memory_order_release);   // even: data before new seq
    }

    T load(long* retries = nullptr) const {              // any number of readers
        std::uint64_t buf[W];
        for (;;) {
            const std::uint64_t s1 = seq_.load(std::memory_order_acquire);
            if (s1 & 1) {                                // writer active: wait
                _mm_pause();
                if (retries) ++*retries;
                continue;
            }
            for (std::size_t i = 0; i < W; ++i)
                buf[i] = words_[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);  // data before 2nd read
            if (seq_.load(std::memory_order_relaxed) == s1) break;  // unchanged: done
            if (retries) ++*retries;
        }
        T out;
        std::memcpy(&out, buf, sizeof(T));
        return out;
    }

    T load_unchecked() const {                           // WRONG on purpose: no protocol
        std::uint64_t buf[W];
        for (std::size_t i = 0; i < W; ++i)
            buf[i] = words_[i].load(std::memory_order_relaxed);
        T out;
        std::memcpy(&out, buf, sizeof(T));
        return out;
    }
};

struct Quote {                                     // invariant: ask = bid + 1,
    std::int64_t bid, ask, size, check;            // size = 3 * bid, check = xor of all
    static Quote make(std::int64_t b) { return {b, b + 1, 3 * b, b ^ (b + 1) ^ (3 * b)}; }
    bool ok() const {
        return ask == bid + 1 && size == 3 * bid && check == (bid ^ ask ^ size);
    }
};

int main() {
    Seqlock<Quote> sl;
    sl.store(Quote::make(1));
    std::atomic<bool> stop{false};
    std::thread writer([&] {                       // writes as fast as it can
        for (std::int64_t b = 2; !stop.load(std::memory_order_relaxed); ++b)
            sl.store(Quote::make(b));
    });
    long torn_raw = 0, torn = 0, retries = 0, same = 0;
    for (long i = 0; i < kReads / 10; ++i) torn_raw += !sl.load_unchecked().ok();
    std::int64_t last = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (long i = 0; i < kReads; ++i) {
        const Quote q = sl.load(&retries);
        torn += !q.ok() || q.bid < last;           // torn, or a version went backwards
        same += (q.bid == last);                   // the same version as the last read
        last = q.bid;
    }
    const std::chrono::duration<double, std::nano> dt =
        std::chrono::steady_clock::now() - t0;
    stop.store(true, std::memory_order_relaxed);
    writer.join();
    std::printf("unchecked reads: %ld of %ld torn\n", torn_raw, kReads / 10);
    std::printf("seqlock reads:   %ld of %ld torn or out of order\n", torn, kReads);
    std::printf("  %.2f retries per read; %.0f%% repeated the previous version\n",
                double(retries) / kReads, 100.0 * double(same) / kReads);
    std::printf("  %.1f ns per read against a writer that never pauses\n",
                dt.count() / kReads);
    return torn == 0 ? 0 : 1;
}
