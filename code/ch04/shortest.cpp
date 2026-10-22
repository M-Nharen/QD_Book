// Shortest paths: Dijkstra with stale heap entries, Bellman-Ford, Floyd-Warshall (4.3).
#include <algorithm>
#include <cstdio>
#include <functional>
#include <queue>
#include <random>
#include <utility>
#include <vector>
using ll = long long;
constexpr ll INF = 1'000'000'000'000'000'000LL;    // 10^18: INF + any path still fits
using Adj = std::vector<std::vector<std::pair<int, ll>>>;   // (to, weight)

std::vector<ll> dijkstra(const Adj& g, int s) {    // requires non-negative weights
    std::vector<ll> d(g.size(), INF);
    using Item = std::pair<ll, int>;               // (distance, vertex), smallest first
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    d[s] = 0;
    pq.push({0, s});
    while (!pq.empty()) {
        const auto [dv, v] = pq.top();
        pq.pop();
        if (dv != d[v]) continue;                  // stale entry: v was improved later
        for (auto [u, w] : g[v])
            if (dv + w < d[u]) {
                d[u] = dv + w;
                pq.push({d[u], u});                // no decrease-key: push a duplicate
            }
    }
    return d;
}

// Bellman-Ford: n-1 rounds of relaxing every edge. Returns false if a negative cycle is
// reachable from s. Stops early when a round changes nothing.
bool bellman_ford(const Adj& g, int s, std::vector<ll>& d) {
    const int n = int(g.size());
    d.assign(n, INF);
    d[s] = 0;
    for (int round = 0; round < n; ++round) {
        bool changed = false;
        for (int v = 0; v < n; ++v) {
            if (d[v] == INF) continue;             // never relax from "infinity"
            for (auto [u, w] : g[v])
                if (d[v] + w < d[u]) { d[u] = d[v] + w; changed = true; }
        }
        if (!changed) return true;
        if (round == n - 1) return false;          // still improving in round n: a cycle
    }
    return true;
}

// Floyd-Warshall: all pairs in O(n^3). k must be the OUTER loop.
std::vector<std::vector<ll>> floyd(const Adj& g) {
    const int n = int(g.size());
    std::vector<std::vector<ll>> d(n, std::vector<ll>(n, INF));
    for (int v = 0; v < n; ++v) {
        d[v][v] = 0;
        for (auto [u, w] : g[v]) d[v][u] = std::min(d[v][u], w);
    }
    for (int k = 0; k < n; ++k)                    // paths using intermediates 0..k
        for (int i = 0; i < n; ++i) {
            if (d[i][k] == INF) continue;
            for (int j = 0; j < n; ++j)
                if (d[k][j] != INF) d[i][j] = std::min(d[i][j], d[i][k] + d[k][j]);
        }
    return d;
}

int main() {
    Adj g(5);
    auto add = [&](int a, int b, ll w) { g[a].push_back({b, w}); };
    add(0, 1, 4); add(0, 2, 1); add(2, 1, 2); add(1, 3, 1); add(2, 3, 5); add(3, 4, 3);
    std::printf("dijkstra from 0:");
    for (ll x : dijkstra(g, 0)) std::printf(" %lld", x);
    std::printf("\n");

    Adj neg(3);                                    // 0 -> 1 -> 2 -> 1 with total -1
    neg[0].push_back({1, 1}); neg[1].push_back({2, 1}); neg[2].push_back({1, -2});
    std::vector<ll> d;
    std::printf("negative cycle detected: %s\n", bellman_ford(neg, 0, d) ? "no" : "yes");

    std::mt19937 rng(23);                          // all three must agree
    int bad = 0;
    for (int t = 0; t < 300; ++t) {
        const int n = 2 + int(rng() % 30);
        Adj h(n);
        const int m = int(rng() % (4 * n));
        for (int e = 0; e < m; ++e)
            h[rng() % n].push_back({int(rng() % n), ll(rng() % 20)});
        const auto fw = floyd(h);
        const int s = int(rng() % n);
        const auto dj = dijkstra(h, s);
        std::vector<ll> bf;
        bad += !bellman_ford(h, s, bf);
        for (int v = 0; v < n; ++v) bad += (dj[v] != fw[s][v]) + (bf[v] != fw[s][v]);
    }
    std::printf("random cross-check (300 graphs): %d mismatches\n", bad);
}
