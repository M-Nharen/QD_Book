// Traversal and lookup cost by container (Section 10.9). Build-container numbers only.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <list>
#include <map>
#include <random>
#include <set>
#include <unordered_map>
#include <vector>
#include "flat_hash.hpp"

volatile std::uint64_t g_sink;

template <class F>
double ns_per(std::size_t n, F&& f) {          // best of 3 runs of f(), per item
    double best = 1e30;
    for (int r = 0; r < 3; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        g_sink = f();
        const auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    return best / double(n);
}

template <class C>
std::uint64_t sum(const C& c) {
    std::uint64_t s = 0;
    for (auto x : c) s += x;
    return s;
}

void traversal(std::size_t n) {
    std::vector<std::uint64_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = i;
    std::deque<std::uint64_t> d(v.begin(), v.end());
    std::list<std::uint64_t> seq(v.begin(), v.end());
    std::list<std::uint64_t> shuffled;          // same values, nodes linked in random order
    {
        std::list<std::uint64_t> tmp(v.begin(), v.end());
        std::vector<std::list<std::uint64_t>::iterator> its;
        for (auto it = tmp.begin(); it != tmp.end(); ++it) its.push_back(it);
        std::shuffle(its.begin(), its.end(), std::mt19937_64{1});
        for (auto it : its) shuffled.splice(shuffled.end(), tmp, it);
    }
    std::set<std::uint64_t> s(v.begin(), v.end());
    const double tv = ns_per(n, [&] { return sum(v); });
    const double td = ns_per(n, [&] { return sum(d); });
    const double tl = ns_per(n, [&] { return sum(seq); });
    const double tls = ns_per(n, [&] { return sum(shuffled); });
    const double ts = ns_per(n, [&] { return sum(s); });
    std::printf("Traversal (sum of %zu uint64), ns per element:\n", n);
    std::printf("  vector %.2f | deque %.2f | list, nodes in order %.2f | "
                "list, nodes shuffled %.2f | set %.2f\n", tv, td, tl, tls, ts);
}

void lookup(std::size_t n, std::size_t queries) {
    std::mt19937_64 rng(2);
    std::vector<std::uint64_t> keys(n);
    for (auto& k : keys) k = rng();
    std::vector<std::uint64_t> q(queries);
    for (auto& x : q) x = keys[rng() % n];      // every query hits
    std::vector<std::uint64_t> sorted = keys;
    std::sort(sorted.begin(), sorted.end());
    std::map<std::uint64_t, std::uint64_t> m;
    std::unordered_map<std::uint64_t, std::uint64_t> um;
    FlatHashMap<std::uint64_t, std::uint64_t> fm;
    for (auto k : keys) { m.emplace(k, k); um.emplace(k, k); fm.try_emplace(k, k); }

    const double t_sorted = ns_per(queries, [&] {
        std::uint64_t s = 0;
        for (auto x : q) s += *std::lower_bound(sorted.begin(), sorted.end(), x);
        return s;
    });
    const double t_map = ns_per(queries, [&] {
        std::uint64_t s = 0;
        for (auto x : q) s += m.find(x)->second;
        return s;
    });
    const double t_um = ns_per(queries, [&] {
        std::uint64_t s = 0;
        for (auto x : q) s += um.find(x)->second;
        return s;
    });
    const double t_fm = ns_per(queries, [&] {
        std::uint64_t s = 0;
        for (auto x : q) s += *fm.find(x);
        return s;
    });
    std::printf("Lookup of random present keys, n=%zu, ns per lookup:\n", n);
    std::printf("  sorted vector + lower_bound %.1f | std::map %.1f | "
                "std::unordered_map %.1f | FlatHashMap %.1f\n",
                t_sorted, t_map, t_um, t_fm);
}

int main() {
    traversal(1 << 20);
    lookup(1'000, 1 << 21);
    lookup(1 << 20, 1 << 21);
}
