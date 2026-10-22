// std::map in practice: ordered queries, hinted insertion, node handles (Section 10.3).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>

static long g_allocs = 0;
[[gnu::noinline]] void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct CountingLess {                         // counts key comparisons
    static inline long n = 0;
    bool operator()(std::uint64_t a, std::uint64_t b) const { ++n; return a < b; }
};

using Px = std::int64_t;
using Qty = std::int64_t;

int main() {
    // Ask levels: lowest price is best, so the default std::less order works.
    std::map<Px, Qty> asks;
    for (Px p = 10'010; p <= 10'050; p += 10) asks.emplace(p, 100);

    // Ordered queries are what a tree is for: best level, and all levels up to a limit.
    std::printf("best ask %lld; levels <= 10030:",
                static_cast<long long>(asks.begin()->first));
    for (auto it = asks.begin(); it != asks.upper_bound(10'030); ++it)
        std::printf(" %lld", static_cast<long long>(it->first));
    std::printf("\n");

    // Erasing while iterating: erase returns the next valid iterator.
    for (auto it = asks.begin(); it != asks.end();)
        it = (it->first % 20 == 0) ? asks.erase(it) : std::next(it);
    std::printf("after erasing multiples of 20: %zu levels\n", asks.size());

    // Hinted insertion: if the new key belongs right before the hint, insertion is
    // amortized O(1) instead of O(log n). Increasing keys: hint = end().
    std::map<std::uint64_t, int, CountingLess> plain, hinted;
    for (std::uint64_t id = 0; id < 100'000; ++id) plain.emplace(id, 0);
    const long c_plain = CountingLess::n;
    CountingLess::n = 0;
    for (std::uint64_t id = 0; id < 100'000; ++id) hinted.emplace_hint(hinted.end(), id, 0);
    std::printf("100000 increasing keys: comparisons plain=%ld (%.1f/insert), "
                "hinted=%ld (%.1f/insert)\n", c_plain, c_plain / 1e5,
                CountingLess::n, CountingLess::n / 1e5);

    // Node handles (C++17): move a node between maps, or re-key it, without freeing
    // and reallocating. A price-level map can recycle nodes this way.
    std::map<Px, Qty> spare;
    const long a0 = g_allocs;
    auto node = asks.extract(10'010);         // unlink: no deallocation
    node.key() = 10'005;                      // change the key in place
    asks.insert(std::move(node));             // relink: no allocation
    auto n2 = asks.extract(10'030);
    spare.insert(std::move(n2));              // moved to another map: no allocation
    std::printf("re-keyed 10010 -> 10005 and moved 10030 to spare: allocations=%ld\n",
                g_allocs - a0);
    std::printf("asks:");
    for (const auto& [p, q] : asks) std::printf(" %lld", static_cast<long long>(p));
    std::printf(" | spare: %lld\n", static_cast<long long>(spare.begin()->first));
}
