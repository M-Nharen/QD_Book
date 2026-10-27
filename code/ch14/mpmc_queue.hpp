// A bounded multi-producer multi-consumer queue with per-slot sequence numbers
// (Section 14.6). The design is Dmitry Vyukov's bounded MPMC queue.
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

template <class T>
class MpmcQueue {
    static_assert(std::is_nothrow_move_assignable_v<T>);
    struct alignas(64) Slot {
        std::atomic<std::uint64_t> seq;    // which lap, and full or empty, this slot is in
        T value;
    };
    const std::uint64_t mask_;
    const std::unique_ptr<Slot[]> slots_;
    alignas(64) std::atomic<std::uint64_t> enq_{0};   // next position to write
    alignas(64) std::atomic<std::uint64_t> deq_{0};   // next position to read

public:
    explicit MpmcQueue(std::size_t capacity)          // capacity: a power of two >= 2
        : mask_(capacity - 1), slots_(new Slot[capacity]) {
        for (std::size_t i = 0; i < capacity; ++i)
            slots_[i].seq.store(i, std::memory_order_relaxed);   // slot i: free for lap 0
    }

    bool try_push(T v) {
        std::uint64_t pos = enq_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& s = slots_[pos & mask_];
            const std::uint64_t seq = s.seq.load(std::memory_order_acquire);
            const auto diff = std::int64_t(seq - pos);
            if (diff == 0) {                                 // free for this position
                if (enq_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;                                   // we own position pos
            } else if (diff < 0) {
                return false;                                // a lap behind: full
            } else {
                pos = enq_.load(std::memory_order_relaxed);  // someone took it: retry
            }
        }
        Slot& s = slots_[pos & mask_];
        s.value = std::move(v);
        s.seq.store(pos + 1, std::memory_order_release);     // publish: full for pos
        return true;
    }

    bool try_pop(T& out) {
        std::uint64_t pos = deq_.load(std::memory_order_relaxed);
        for (;;) {
            Slot& s = slots_[pos & mask_];
            const std::uint64_t seq = s.seq.load(std::memory_order_acquire);
            const auto diff = std::int64_t(seq - (pos + 1));
            if (diff == 0) {                                 // full for this position
                if (deq_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (diff < 0) {
                return false;                                // not yet written: empty
            } else {
                pos = deq_.load(std::memory_order_relaxed);
            }
        }
        Slot& s = slots_[pos & mask_];
        out = std::move(s.value);
        s.seq.store(pos + mask_ + 1, std::memory_order_release);  // free for next lap
        return true;
    }
};
