// Showdown (Section 10.8): an order-id -> order index under a feed-like workload.
// std::map vs std::unordered_map vs FlatHashMap (Listing 10.5), same operation stream.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>
#include <unordered_map>
#include <vector>
#include "flat_hash.hpp"

static long g_allocs = 0;                     // counts every operator new call
[[gnu::noinline]] void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc{};
}
[[gnu::noinline]] void operator delete(void* p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}

struct Order { std::int64_t px; std::uint32_t qty; std::uint32_t symbol; char side; };

enum class Op : std::uint8_t { Add, Cancel, Modify };
struct Event { Op op; std::uint64_t id; };

// Builds a reproducible stream: prefill N live orders, then random add/cancel/modify on
// live ids. Exchange order ids increase monotonically, as on most feeds.
std::vector<Event> make_stream(std::size_t live_target, std::size_t n_events) {
    std::vector<Event> ev;
    std::vector<std::uint64_t> live;
    std::uint64_t next_id = 1'000'000, s = 12345;
    auto rnd = [&s] {
        std::uint64_t z = (s += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    };
    while (live.size() < live_target) {
        ev.push_back({Op::Add, next_id});
        live.push_back(next_id++);
    }
    while (ev.size() < live_target + n_events) {
        const unsigned r = rnd() % 100;
        if (r < 40 || live.empty()) {
            ev.push_back({Op::Add, next_id}); live.push_back(next_id++);
        } else {
            const std::size_t i = rnd() % live.size();
            if (r < 80) {                                  // cancel: swap-remove from live
                ev.push_back({Op::Cancel, live[i]});
                live[i] = live.back(); live.pop_back();
            } else {
                ev.push_back({Op::Modify, live[i]});
            }
        }
    }
    return ev;
}

template <class Map, class Insert, class Find, class Erase>
void replay(const char* name, const std::vector<Event>& ev, std::size_t prefill,
            Map& m, Insert ins, Find find, Erase erase) {
    for (std::size_t i = 0; i < prefill; ++i) ins(m, ev[i].id, Order{100, 1, 7, 'B'});
    const long a0 = g_allocs;
    std::uint64_t checksum = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = prefill; i < ev.size(); ++i) {
        const Event& e = ev[i];
        switch (e.op) {
        case Op::Add:    ins(m, e.id, Order{100 + std::int64_t(i & 63), 10, 7, 'B'}); break;
        case Op::Cancel: checksum += erase(m, e.id); break;
        case Op::Modify: if (Order* o = find(m, e.id)) { o->qty += 1; checksum += o->qty; }
                         break;
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double n = double(ev.size() - prefill);
    std::printf("%-34s %6.1f ns/op  %.3f allocs/op  (checksum %llu)\n", name,
                std::chrono::duration<double, std::nano>(t1 - t0).count() / n,
                double(g_allocs - a0) / n, static_cast<unsigned long long>(checksum));
}

int main() {
    for (std::size_t live : {10'000u, 1'000'000u}) {
        const auto ev = make_stream(live, 4'000'000);
        std::printf("%zu live orders, 4M events (40%% add, 40%% cancel, 20%% modify)\n",
                    live);
        {
            std::map<std::uint64_t, Order> m;
            replay("std::map", ev, live, m,
                [](auto& c, std::uint64_t k, const Order& o) { c.emplace(k, o); },
                [](auto& c, std::uint64_t k) -> Order* {
                    auto it = c.find(k); return it == c.end() ? nullptr : &it->second; },
                [](auto& c, std::uint64_t k) { return c.erase(k); });
        }
        {
            std::unordered_map<std::uint64_t, Order> m;
            m.reserve(live + live / 4);
            replay("std::unordered_map (reserved)", ev, live, m,
                [](auto& c, std::uint64_t k, const Order& o) { c.emplace(k, o); },
                [](auto& c, std::uint64_t k) -> Order* {
                    auto it = c.find(k); return it == c.end() ? nullptr : &it->second; },
                [](auto& c, std::uint64_t k) { return c.erase(k); });
        }
        {
            FlatHashMap<std::uint64_t, Order> m;
            m.reserve(live + live / 4);
            replay("FlatHashMap (reserved)", ev, live, m,
                [](auto& c, std::uint64_t k, const Order& o) { c.try_emplace(k, o); },
                [](auto& c, std::uint64_t k) { return c.find(k); },
                [](auto& c, std::uint64_t k) { return std::size_t{c.erase(k)}; });
        }
    }
}
