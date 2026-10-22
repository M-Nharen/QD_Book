// The same Dijkstra on flat arrays: CSR graph, vector distances, heap with stale entries.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <queue>
#include <utility>
#include <vector>
using ll = long long;
constexpr ll INF = 1'000'000'000'000'000'000LL;

struct Arc { int to, w; };                         // 8 bytes: eight arcs per 64-byte line
struct CSR {
    std::vector<int> off;                          // arcs of v: arc[off[v] .. off[v+1])
    std::vector<Arc> arc;
};

CSR make_graph(int n, int m) {                     // same edges as Listing 4.10
    std::vector<int> from(m);
    std::vector<Arc> tmp(m);
    std::uint64_t x = 88172645463325252ULL;
    auto next = [&] { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return x; };
    for (int e = 0; e < m; ++e) {
        from[e] = int(next() % unsigned(n));
        tmp[e].to = int(next() % unsigned(n));
        tmp[e].w = int(1 + next() % 1000);
    }
    CSR g{std::vector<int>(n + 1, 0), std::vector<Arc>(m)};
    for (int e = 0; e < m; ++e) ++g.off[from[e] + 1];    // counting sort by source
    for (int v = 0; v < n; ++v) g.off[v + 1] += g.off[v];
    std::vector<int> pos(g.off.begin(), g.off.end() - 1);
    for (int e = 0; e < m; ++e) g.arc[pos[from[e]]++] = tmp[e];   // keeps input order
    return g;
}

std::vector<ll> dijkstra(const CSR& g, int s) {    // by const reference: no copy
    const int n = int(g.off.size()) - 1;
    std::vector<ll> dist(n, INF);
    using Item = std::pair<ll, int>;
    std::vector<Item> storage;
    storage.reserve(g.arc.size() + 1);             // at most one push per relaxation
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq(
        std::greater<Item>{}, std::move(storage));
    dist[s] = 0;
    pq.push({0, s});
    while (!pq.empty()) {
        const auto [dv, v] = pq.top();
        pq.pop();
        if (dv != dist[v]) continue;               // stale: v was already settled cheaper
        for (int i = g.off[v]; i < g.off[v + 1]; ++i) {   // contiguous scan of v's arcs
            const Arc a = g.arc[i];
            if (dv + a.w < dist[a.to]) {
                dist[a.to] = dv + a.w;
                pq.push({dist[a.to], a.to});
            }
        }
    }
    return dist;
}

int main() {
    const int n = 200'000, m = 1'000'000;
    const CSR g = make_graph(n, m);
    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<ll> dist = dijkstra(g, 0);
    const auto t1 = std::chrono::steady_clock::now();
    ll sum = 0;
    std::size_t reached = 0;
    for (ll d : dist)
        if (d < INF) { sum += d; ++reached; }
    std::printf("reached %zu vertices, sum of distances %lld, %.1f ms\n", reached, sum,
                std::chrono::duration<double, std::milli>(t1 - t0).count());
}
