// book: flags=-pthread
// The ABA problem in a lock-free stack (Treiber stack) used as a free list of pool
// slots, and the tagged-index fix (Section 14.8). Also run under tsan: no races.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

constexpr std::uint32_t kNil = 0xffff'ffff;

// Head word = (tag << 32) | index. Without tags, the tag stays 0.
template <bool Tagged>
class FreeList {
public:
    struct Node {
        std::atomic<std::uint32_t> next{kNil};
        long payload = 0;                  // owned by whoever popped the node
    };
    explicit FreeList(std::uint32_t n) : nodes_(n) {
        for (std::uint32_t i = n; i-- > 0;) push(i);   // stack: 0 on top, then 1, 2...
    }
    Node& node(std::uint32_t i) { return nodes_[i]; }

    void push(std::uint32_t i) {
        std::uint64_t h = head_.load(std::memory_order_relaxed);
        do {
            nodes_[i].next.store(std::uint32_t(h), std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(h, bump(h, i), std::memory_order_release,
                                              std::memory_order_relaxed));
    }

    // pop, split in two so that a test can pause a thread between the halves.
    struct Snapshot { std::uint64_t head; std::uint32_t next; };
    Snapshot pop_begin() {
        const std::uint64_t h = head_.load(std::memory_order_acquire);
        const std::uint32_t top = std::uint32_t(h);
        if (top == kNil) return {h, kNil};
        return {h, nodes_[top].next.load(std::memory_order_relaxed)};  // may be stale!
    }
    std::uint32_t pop_finish(Snapshot s) {        // kNil: empty, or lost a race
        if (std::uint32_t(s.head) == kNil) return kNil;
        std::uint64_t expected = s.head;
        if (head_.compare_exchange_strong(expected, bump(s.head, s.next),
                                          std::memory_order_acquire,
                                          std::memory_order_relaxed))
            return std::uint32_t(s.head);
        return kNil;
    }
    std::uint32_t pop() {                         // retry until success or empty
        for (;;) {
            const Snapshot s = pop_begin();
            if (std::uint32_t(s.head) == kNil) return kNil;
            if (const std::uint32_t i = pop_finish(s); i != kNil) return i;
        }
    }

    std::string dump() const {                    // single-threaded use only
        std::string out;
        std::uint32_t i = std::uint32_t(head_.load());
        for (int guard = 0; i != kNil && guard < 10; ++guard) {
            out += char('A' + i);
            out += ' ';
            i = nodes_[i].next.load();
        }
        return out.empty() ? "(empty)" : out;
    }

private:
    static std::uint64_t bump(std::uint64_t old_head, std::uint32_t new_top) {
        const std::uint64_t tag = Tagged ? (old_head >> 32) + 1 : 0;
        return (tag << 32) | new_top;
    }
    std::vector<Node> nodes_;
    std::atomic<std::uint64_t> head_{kNil};
};

template <bool Tagged>
void replay(const char* title) {
    FreeList<Tagged> fl(3);                       // A -> B -> C
    auto show = [&](const std::string& what) {
        std::printf("  %-28s %s\n", what.c_str(), fl.dump().c_str());
    };
    std::printf("%s\n", title);
    show("start:");
    auto t1 = fl.pop_begin();                     // T1 reads head = A, next = B; paused
    const std::uint32_t a = fl.pop();             // T2 pops A
    const std::uint32_t b = fl.pop();             // T2 pops B and is now using it
    fl.push(a);                                   // T2 returns A: head is A again
    show("T2 pops A, pops B, pushes A:");
    const std::uint32_t got = fl.pop_finish(t1);  // T1 resumes: CAS(head: A -> B)
    show(got == kNil ? "T1 resumes, CAS fails:" : "T1 resumes, CAS succeeds:");
    if (got == kNil) {
        const std::uint32_t r = fl.pop();
        show(std::string("T1 retries and pops ") + char('A' + r) + ":");
    } else if (fl.dump()[0] == char('A' + b)) {
        std::printf("  B is free AND in use by T2: the next pop hands it out twice\n");
    }
}

// Concurrent stress of the tagged list: a node must never be owned by two threads.
long stress() {
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
    constexpr long kOps = 100'000;
#else
    constexpr long kOps = 2'000'000;
#endif
    FreeList<true> fl(4);
    std::vector<std::atomic<int>> owner(4);
    std::atomic<long> errors{0};
    auto work = [&](int me) {
        for (long k = 0; k < kOps; ++k) {
            const std::uint32_t i = fl.pop();
            if (i == kNil) continue;              // all four taken right now
            if (owner[i].exchange(me + 1, std::memory_order_relaxed) != 0) ++errors;
            fl.node(i).payload += me;             // plain write: we own the node
            owner[i].store(0, std::memory_order_relaxed);
            fl.push(i);
        }
    };
    std::thread t(work, 1);
    work(0);
    t.join();
    return errors.load();
}

int main() {
    replay<false>("Untagged head (index only):");
    replay<true>("Tagged head (index + 32-bit version):");
    std::printf("concurrent stress, tagged: %ld double allocations\n", stress());
}
