// Differential test of FlatHashMap against std::unordered_map (Section 10.5).
// Random inserts, lookups and erases; every key pattern; object lifetimes counted.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>
#include "flat_hash.hpp"

static long g_live = 0;                       // live Payload objects
struct Payload {                              // non-trivial value type
    std::string s;
    explicit Payload(long v = 0) : s(std::to_string(v)) { ++g_live; }
    Payload(const Payload& o) : s(o.s) { ++g_live; }
    Payload(Payload&& o) noexcept : s(std::move(o.s)) { ++g_live; }
    Payload& operator=(const Payload&) = default;
    Payload& operator=(Payload&&) noexcept = default;
    ~Payload() { --g_live; }
};

#define CHECK(cond)                                                            \
    do { if (!(cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #cond);   \
                        std::exit(1); } } while (0)

std::uint64_t rng_state = 7;
std::uint64_t next() {                        // SplitMix64
    std::uint64_t z = (rng_state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

// keyspace: how keys are drawn; stride: multiply keys to create adversarial patterns
void run(const char* name, std::uint64_t keyspace, std::uint64_t stride, int ops) {
    FlatHashMap<std::uint64_t, Payload> m;
    std::unordered_map<std::uint64_t, long> ref;
    long inserts = 0, erases = 0, hits = 0;
    for (int op = 0; op < ops; ++op) {
        const std::uint64_t k = (next() % keyspace) * stride;
        const unsigned r = next() % 10;
        if (r < 5) {                                         // insert
            const long v = static_cast<long>(next() % 1'000'000);
            const auto [p, inserted] = m.try_emplace(k, v);
            const bool ref_inserted = ref.emplace(k, v).second;
            CHECK(inserted == ref_inserted);
            CHECK(p->s == std::to_string(ref.at(k)));
            inserts += inserted;
        } else if (r < 8) {                                  // erase
            const bool e = m.erase(k);
            CHECK(e == (ref.erase(k) == 1));
            erases += e;
        } else {                                             // lookup
            const Payload* p = m.find(k);
            const auto it = ref.find(k);
            CHECK((p != nullptr) == (it != ref.end()));
            if (p) { CHECK(p->s == std::to_string(it->second)); ++hits; }
        }
        CHECK(m.size() == ref.size());
        CHECK(static_cast<std::size_t>(g_live) == ref.size());
    }
    for (const auto& [k, v] : ref) {                         // every key still reachable
        const Payload* p = m.find(k);
        CHECK(p != nullptr && p->s == std::to_string(v));
    }
    std::size_t visited = 0;
    m.for_each([&](const std::uint64_t& k, Payload&) { ++visited; CHECK(ref.count(k)); });
    CHECK(visited == ref.size());
    std::printf("%-30s ops=%d inserts=%ld erases=%ld hits=%ld final size=%zu cap=%zu\n",
                name, ops, inserts, erases, hits, m.size(), m.capacity());
}

int main() {
    run("random keys, small keyspace", 500, 1, 200'000);    // heavy churn, long clusters
    run("random keys, large keyspace", 1u << 30, 1, 200'000);
    run("multiples of 4096 (weak hash)", 20'000, 4096, 200'000);
    run("dense ids 0..99", 100, 1, 100'000);                // constant reuse of slots
    CHECK(g_live == 0);                                      // no leaks, no double free

    FlatHashMap<std::string, int> names;                     // non-trivial keys
    for (int i = 0; i < 1000; ++i) names["sym" + std::to_string(i)] = i;
    for (int i = 0; i < 1000; i += 2) CHECK(names.erase("sym" + std::to_string(i)));
    CHECK(names.size() == 500 && !names.contains("sym0") && *names.find("sym999") == 999);
    FlatHashMap<std::string, int> moved = std::move(names);  // move leaves source empty
    CHECK(moved.size() == 500 && names.size() == 0 && names.find("sym1") == nullptr);
    moved.clear();
    CHECK(moved.empty() && moved.find("sym1") == nullptr);
    std::printf("string keys, move and clear: ok\nall tests passed\n");
}
