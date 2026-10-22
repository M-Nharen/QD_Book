// Dijkstra as it is often first written: correct, O((n + m) log n), and slow (4.8).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <utility>
#include <vector>
using ll = long long;
using Graph = std::map<int, std::vector<std::pair<int, int>>>;   // vertex -> (to, weight)

Graph make_graph(int n, int m) {                   // deterministic random directed graph
    Graph g;
    std::uint64_t x = 88172645463325252ULL;        // xorshift64: same sequence everywhere
    auto next = [&] { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return x; };
    for (int e = 0; e < m; ++e) {
        const int a = int(next() % unsigned(n)), b = int(next() % unsigned(n));
        g[a].push_back({b, int(1 + next() % 1000)});
    }
    return g;
}

std::map<int, ll> dijkstra(Graph g, int s) {       // graph passed BY VALUE: a full copy
    std::map<int, ll> dist;                        // missing key means "infinity"
    std::set<std::pair<ll, int>> frontier;         // ordered set doubles as the heap
    dist[s] = 0;
    frontier.insert({0, s});
    while (!frontier.empty()) {
        const auto [dv, v] = *frontier.begin();
        frontier.erase(frontier.begin());
        for (auto [u, w] : g[v]) {                 // operator[] on a map: a tree search
            const ll nd = dv + w;
            auto it = dist.find(u);
            if (it == dist.end() || nd < it->second) {
                if (it != dist.end()) frontier.erase({it->second, u});   // "decrease key"
                dist[u] = nd;
                frontier.insert({nd, u});
            }
        }
    }
    return dist;
}

int main() {
    const int n = 200'000, m = 1'000'000;
    const Graph g = make_graph(n, m);
    const auto t0 = std::chrono::steady_clock::now();
    const std::map<int, ll> dist = dijkstra(g, 0);
    const auto t1 = std::chrono::steady_clock::now();
    ll sum = 0;
    for (const auto& [v, d] : dist) sum += d;
    std::printf("reached %zu vertices, sum of distances %lld, %.1f ms\n", dist.size(), sum,
                std::chrono::duration<double, std::milli>(t1 - t0).count());
}
