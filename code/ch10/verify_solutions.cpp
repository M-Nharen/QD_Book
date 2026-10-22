// Checks the numeric answers to the Chapter 10 practice problems (Appendix A, A.10).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <list>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "flat_hash.hpp"

static long g_allocs = 0;
[[gnu::noinline]] void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}

std::uint64_t g_s = 99;
std::uint64_t rnd() {
    std::uint64_t z = (g_s += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// ---- P10.1
struct Mv {
    static inline long moves = 0;
    int v;
    explicit Mv(int x) : v(x) {}
    Mv(Mv&& o) noexcept : v(o.v) { ++moves; }
};
void p10_1() {
    std::vector<Mv> v;
    long allocs = 0;
    for (int i = 0; i < 1000; ++i) {
        const auto cap = v.capacity();
        v.emplace_back(i);
        allocs += v.capacity() != cap;
    }
    std::printf("P10.1 libstdc++ (2x): allocations=%ld element moves=%ld\n", allocs,
                Mv::moves);
    // Model of a 1.5x rule: new_cap = max(cap + cap / 2, size + 1).
    long cap = 0, a = 0, moves = 0;
    for (long size = 0; size < 1000; ++size)
        if (size == cap) { moves += size; cap = std::max(cap + cap / 2, size + 1); ++a; }
    std::printf("      1.5x model:     allocations=%ld element moves=%ld final cap=%ld\n",
                a, moves, cap);
}

// ---- P10.5: LRU cache, LeetCode 146 example sequence
class LRU {
public:
    explicit LRU(std::size_t cap) : cap_(cap) {}
    int get(int k) {
        auto it = pos_.find(k);
        if (it == pos_.end()) return -1;
        order_.splice(order_.begin(), order_, it->second);   // move to front, O(1)
        return it->second->second;
    }
    void put(int k, int v) {
        if (auto it = pos_.find(k); it != pos_.end()) {
            it->second->second = v;
            order_.splice(order_.begin(), order_, it->second);
            return;
        }
        if (order_.size() == cap_) {                          // evict least recent
            pos_.erase(order_.back().first);
            order_.pop_back();
        }
        order_.emplace_front(k, v);
        pos_[k] = order_.begin();
    }
private:
    std::size_t cap_;
    std::list<std::pair<int, int>> order_;                    // front = most recent
    std::unordered_map<int, std::list<std::pair<int, int>>::iterator> pos_;
};
void p10_5() {
    LRU c(2);
    c.put(1, 1); c.put(2, 2);
    const int a = c.get(1);
    c.put(3, 3);
    const int b = c.get(2);
    c.put(4, 4);
    std::printf("P10.5 LRU: get(1)=%d get(2)=%d get(1)=%d get(3)=%d get(4)=%d\n",
                a, b, c.get(1), c.get(3), c.get(4));
}

// ---- P10.6: Knuth's formulas vs FlatHashMap, capacity 65536
void p10_6() {
    for (double alpha : {0.25, 0.5, 0.75}) {
        FlatHashMap<std::uint64_t, int> m;
        m.reserve(49'152);                                   // capacity 65536
        const std::size_t n = static_cast<std::size_t>(alpha * 65'536);
        std::vector<std::uint64_t> keys(n);
        for (auto& k : keys) { k = rnd(); m.try_emplace(k, 0); }
        double hit = 0, miss = 0;
        for (auto k : keys) hit += double(m.probe_length(k));
        const int misses = 200'000;
        for (int i = 0; i < misses; ++i) miss += double(m.probe_length(rnd()));
        std::printf("P10.6 alpha=%.2f cap=%zu  hit %.3f (Knuth %.3f)  "
                    "miss %.3f (Knuth %.3f)\n", alpha, m.capacity(), hit / double(n),
                    0.5 * (1 + 1 / (1 - alpha)),
                    miss / misses, 0.5 * (1 + 1 / ((1 - alpha) * (1 - alpha))));
    }
}

// ---- P10.7: identity hash, mask indexing, keys i * 4096, no mixing
void p10_7() {
    const std::size_t cap = 65'536, mask = cap - 1, n = 10'000;
    std::vector<std::uint64_t> slot_key(cap, ~0ULL);
    std::vector<char> used(cap, 0);
    double total = 0;
    for (std::uint64_t i = 0; i < n; ++i) {
        const std::uint64_t k = i * 4096;
        std::size_t j = k & mask, probes = 1;
        while (used[j]) { j = (j + 1) & mask; ++probes; }
        used[j] = 1; slot_key[j] = k;
        total += double(probes);   // probes to insert = probes to find it later
    }
    std::size_t homes = 0;
    for (std::size_t s = 0; s < cap; s += 1) homes += used[s] && (slot_key[s] & mask) == s;
    std::printf("P10.7 no mixing: distinct home slots=%zu, mean successful probes=%.1f\n",
                homes, total / n);
    FlatHashMap<std::uint64_t, int> m;
    m.reserve(49'152);
    for (std::uint64_t i = 0; i < n; ++i) m.try_emplace(i * 4096, 0);
    double t = 0;
    for (std::uint64_t i = 0; i < n; ++i) t += double(m.probe_length(i * 4096));
    std::printf("      FlatHashMap (mixed): mean successful probes=%.3f\n", t / n);
}

// ---- P10.8: heterogeneous lookup
struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};
void p10_8() {
    std::unordered_map<std::string, int, StringHash, std::equal_to<>> m;
    m.emplace("XNSE:RELIANCE-FUTURES-2026-12", 1);          // longer than the SSO buffer
    std::unordered_map<std::string, int> plain;
    plain.emplace("XNSE:RELIANCE-FUTURES-2026-12", 1);
    const std::string_view key = "XNSE:RELIANCE-FUTURES-2026-12";
    long a0 = g_allocs;
    const bool f1 = m.find(key) != m.end();
    const long transparent_allocs = g_allocs - a0;
    a0 = g_allocs;
    const bool f2 = plain.find(std::string(key)) != plain.end();
    const long plain_allocs = g_allocs - a0;
    std::printf("P10.8 found=%d/%d allocations per find: transparent=%ld, "
                "std::string=%ld\n", f1, f2, transparent_allocs, plain_allocs);
}

// ---- P10.10: Robin Hood vs plain linear probing, load 0.75, displacement statistics
void p10_10() {
    const std::size_t cap = 65'536, mask = cap - 1, n = 49'152;
    std::vector<std::uint64_t> h(n);
    for (auto& x : h) x = rnd();
    for (int robin = 0; robin < 2; ++robin) {
        std::vector<std::uint64_t> tab(cap);
        std::vector<char> used(cap, 0);
        for (std::uint64_t x : h) {
            std::size_t j = x & mask, d = 0;                    // d: displacement of x
            while (used[j]) {
                const std::size_t dj = (j - (tab[j] & mask)) & mask;
                if (robin && dj < d) { std::swap(x, tab[j]); d = dj; }  // take from rich
                j = (j + 1) & mask; ++d;
            }
            used[j] = 1; tab[j] = x;
        }
        double sum = 0, sum2 = 0; std::size_t mx = 0;
        for (std::size_t j = 0; j < cap; ++j) {
            if (!used[j]) continue;
            const std::size_t d = (j - (tab[j] & mask)) & mask;
            sum += double(d); sum2 += double(d) * double(d); mx = std::max(mx, d);
        }
        const double mean = sum / n;
        std::printf("P10.10 %-11s mean displacement %.3f  std dev %.2f  max %zu\n",
                    robin ? "Robin Hood" : "linear", mean,
                    std::sqrt(sum2 / n - mean * mean), mx);
    }
}

int main() {
    p10_1();
    p10_5();
    p10_6();
    p10_7();
    p10_8();
    p10_10();
}
