// Inside libstdc++'s std::unordered_map: hashes, buckets, rehashing, chains (Section 10.4).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_map>

template <class Map>
void chain_stats(const char* name, const Map& m) {
    std::size_t longest = 0, empty = 0;
    for (std::size_t b = 0; b < m.bucket_count(); ++b) {
        longest = std::max(longest, m.bucket_size(b));
        empty += m.bucket_size(b) == 0;
    }
    std::printf("%-34s size=%zu buckets=%zu load=%.2f longest chain=%zu empty=%.0f%%\n",
                name, m.size(), m.bucket_count(), m.load_factor(), longest,
                100.0 * double(empty) / double(m.bucket_count()));
}

int main() {
    // 1. std::hash for integers is the identity in libstdc++.
    std::printf("std::hash<int>{}(42) = %zu, std::hash<std::uint64_t>{}(1e12) = %zu\n",
                std::hash<int>{}(42), std::hash<std::uint64_t>{}(1'000'000'000'000));

    // 2. Rehash points: the bucket count jumps to a prime when size exceeds
    //    max_load_factor() * bucket_count() (max_load_factor defaults to 1.0).
    std::unordered_map<std::uint64_t, int> m;
    std::printf("max_load_factor=%.1f; bucket counts:", m.max_load_factor());
    std::size_t last = m.bucket_count();
    std::printf(" %zu", last);
    for (std::uint64_t i = 0; i < 100'000; ++i) {
        m.emplace(i, 0);
        if (m.bucket_count() != last) {
            last = m.bucket_count();
            std::printf(" %zu(@%llu)", last, static_cast<unsigned long long>(m.size()));
        }
    }
    std::printf("\n");
    chain_stats("sequential keys 0..99999", m);

    // 3. Keys that are multiples of the bucket count all land in bucket 0.
    std::unordered_map<std::uint64_t, int> bad;
    bad.reserve(20'000);                     // fixes the bucket count before inserting
    const std::uint64_t B = bad.bucket_count();
    for (std::uint64_t i = 0; i < 5'000; ++i) bad.emplace(i * B, 0);
    chain_stats("multiples of bucket_count", bad);

    // 4. The same keys through a mixing hash spread out again.
    struct Mix {
        std::size_t operator()(std::uint64_t x) const noexcept {
            x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;      // splitmix64 finalizer
            x ^= x >> 27; x *= 0x94d049bb133111ebULL;
            return x ^ (x >> 31);
        }
    };
    std::unordered_map<std::uint64_t, int, Mix> good;
    good.reserve(20'000);
    for (std::uint64_t i = 0; i < 5'000; ++i) good.emplace(i * B, 0);
    chain_stats("same keys, mixing hash", good);

    // 5. Walking one bucket: a bucket is a run of nodes in one singly linked list.
    std::unordered_map<std::string, int> s{{"AAPL", 1}, {"MSFT", 2}, {"NVDA", 3}};
    const auto b = s.bucket("MSFT");
    std::printf("bucket(\"MSFT\")=%zu holds:", b);
    for (auto it = s.begin(b); it != s.end(b); ++it) std::printf(" %s", it->first.c_str());
    std::printf("\n");
}
