// A bounded single-producer single-consumer ring buffer (Section 14.5).
// Exactly one thread may call try_push and exactly one (other) thread try_pop.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

inline constexpr std::size_t kLine = 64;   // not hardware_destructive_interference_size:
                                           // see the text on -Winterference-size
template <class T, std::size_t N>
class SpscRing {
    static_assert(N >= 2 && (N & (N - 1)) == 0, "capacity must be a power of two");
    static_assert(std::is_trivially_copyable_v<T>, "slots are overwritten in place");
    static constexpr std::uint64_t kMask = N - 1;

    // Producer's line: tail_ is written only by the producer, read by the consumer.
    alignas(kLine) std::atomic<std::uint64_t> tail_{0};
    std::uint64_t head_cache_ = 0;         // producer-private copy of head_
    // Consumer's line: head_ is written only by the consumer, read by the producer.
    alignas(kLine) std::atomic<std::uint64_t> head_{0};
    std::uint64_t tail_cache_ = 0;         // consumer-private copy of tail_
    alignas(kLine) T slots_[N];

public:
    bool try_push(const T& v) {                                    // producer only
        const std::uint64_t t = tail_.load(std::memory_order_relaxed);   // our own index
        if (t - head_cache_ == N) {                                // looks full:
            head_cache_ = head_.load(std::memory_order_acquire);   // refresh, once
            if (t - head_cache_ == N) return false;                // really full
        }
        slots_[t & kMask] = v;                                     // write the slot...
        tail_.store(t + 1, std::memory_order_release);             // ...then publish it
        return true;
    }

    bool try_pop(T& out) {                                         // consumer only
        const std::uint64_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_cache_) {                                    // looks empty:
            tail_cache_ = tail_.load(std::memory_order_acquire);
            if (h == tail_cache_) return false;                    // really empty
        }
        out = slots_[h & kMask];                                   // read the slot...
        head_.store(h + 1, std::memory_order_release);             // ...then free it
        return true;
    }

    // Approximate while the other side runs. head_ is read first: both indices only
    // grow and head <= tail always, so the difference can never go negative.
    std::size_t size() const {
        const std::uint64_t h = head_.load(std::memory_order_acquire);
        return std::size_t(tail_.load(std::memory_order_acquire) - h);
    }
    static constexpr std::size_t capacity() { return N; }
};
