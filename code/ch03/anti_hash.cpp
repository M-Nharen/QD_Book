// Anti-hash test: why std::unordered_map<long long, ...> can be forced to O(n^2) (3.3).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vector>
using ll = long long;

struct SplitMixHash {                              // a strong, per-run randomized mix
    static std::uint64_t splitmix64(std::uint64_t x) {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }
    std::size_t operator()(ll k) const {
        static const std::uint64_t seed = std::uint64_t(
            std::chrono::steady_clock::now().time_since_epoch().count());
        return splitmix64(std::uint64_t(k) + seed);
    }
};

template <class Map>
double insert_all(const std::vector<ll>& keys, std::size_t& max_bucket) {
    const auto t0 = std::chrono::steady_clock::now();
    Map m;
    for (ll k : keys) ++m[k];
    const auto t1 = std::chrono::steady_clock::now();
    max_bucket = 0;
    for (std::size_t b = 0; b < m.bucket_count(); ++b)
        if (m.bucket_size(b) > max_bucket) max_bucket = m.bucket_size(b);
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main() {
    const int n = 100'000;
    // Step 1 (the attacker): learn the bucket count the table reaches after n inserts.
    std::unordered_map<ll, int> probe;
    for (int i = 0; i < n; ++i) probe[i] = 0;
    const ll B = ll(probe.bucket_count());
    std::printf("bucket count after %d inserts: %lld\n", n, B);

    std::vector<ll> benign(n), evil(n);
    for (int i = 0; i < n; ++i) {
        benign[i] = 1'000'003LL * i;               // arbitrary distinct keys
        evil[i] = B * (i + 1);                     // all are 0 modulo B
    }
    std::size_t mb = 0;
    using Std = std::unordered_map<ll, int>;
    using Mix = std::unordered_map<ll, int, SplitMixHash>;
    double t = insert_all<Std>(benign, mb);
    std::printf("std::hash, benign keys : %8.1f ms  longest bucket %zu\n", t, mb);
    t = insert_all<Std>(evil, mb);
    std::printf("std::hash, evil keys   : %8.1f ms  longest bucket %zu\n", t, mb);
    t = insert_all<Mix>(evil, mb);
    std::printf("splitmix,  evil keys   : %8.1f ms  longest bucket %zu\n", t, mb);
}
