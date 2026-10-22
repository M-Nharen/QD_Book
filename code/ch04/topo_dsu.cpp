// Topological sort (Kahn), disjoint-set union, and Kruskal's minimum spanning tree (4.4).
#include <algorithm>
#include <cstdio>
#include <functional>
#include <numeric>
#include <queue>
#include <random>
#include <tuple>
#include <vector>
using ll = long long;

// Kahn's algorithm. A min-heap instead of a FIFO queue gives the lexicographically
// smallest order. Returns fewer than n vertices if the graph has a cycle.
std::vector<int> topo_order(int n, const std::vector<std::pair<int, int>>& edges) {
    std::vector<std::vector<int>> out(n);
    std::vector<int> indeg(n, 0), order;
    for (auto [a, b] : edges) { out[a].push_back(b); ++indeg[b]; }
    std::priority_queue<int, std::vector<int>, std::greater<int>> ready;
    for (int v = 0; v < n; ++v)
        if (indeg[v] == 0) ready.push(v);
    while (!ready.empty()) {
        const int v = ready.top();
        ready.pop();
        order.push_back(v);
        for (int u : out[v])
            if (--indeg[u] == 0) ready.push(u);    // all prerequisites of u are done
    }
    return order;
}

struct DSU {
    std::vector<int> parent, size;
    explicit DSU(int n) : parent(n), size(n, 1) {
        std::iota(parent.begin(), parent.end(), 0);
    }
    int find(int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];         // path halving: point to grandparent
            x = parent[x];
        }
        return x;
    }
    bool unite(int a, int b) {                     // false if already in the same set
        a = find(a); b = find(b);
        if (a == b) return false;
        if (size[a] < size[b]) std::swap(a, b);    // union by size: smaller under larger
        parent[b] = a;
        size[a] += size[b];
        return true;
    }
};

ll kruskal(int n, std::vector<std::tuple<ll, int, int>> edges, int& used) {
    std::sort(edges.begin(), edges.end());         // by weight
    DSU dsu(n);
    ll total = 0;
    used = 0;
    for (auto [w, a, b] : edges)
        if (dsu.unite(a, b)) { total += w; ++used; }   // cheapest edge joining two parts
    return total;                                  // a spanning tree iff used == n - 1
}

ll prim_dense(const std::vector<std::vector<ll>>& w) {     // O(n^2) reference
    const int n = int(w.size());
    const ll INF = 1LL << 60;
    std::vector<ll> best(n, INF);
    std::vector<bool> in(n, false);
    best[0] = 0;
    ll total = 0;
    for (int it = 0; it < n; ++it) {
        int v = -1;
        for (int u = 0; u < n; ++u)
            if (!in[u] && (v < 0 || best[u] < best[v])) v = u;
        in[v] = true;
        total += best[v];
        for (int u = 0; u < n; ++u)
            if (!in[u]) best[u] = std::min(best[u], w[v][u]);
    }
    return total;
}

int main() {
    const auto order = topo_order(6, {{5, 2}, {5, 0}, {4, 0}, {4, 1}, {2, 3}, {3, 1}});
    std::printf("topological order:");
    for (int v : order) std::printf(" %d", v);
    std::printf("\ncycle check: %zu of 3 vertices ordered\n",
                topo_order(3, {{0, 1}, {1, 2}, {2, 0}}).size());

    DSU d(6);
    d.unite(0, 1); d.unite(2, 3); d.unite(1, 3);
    std::printf("same(0,2)=%d same(0,4)=%d size of 0's set=%d\n", d.find(0) == d.find(2),
                d.find(0) == d.find(4), d.size[d.find(0)]);

    std::mt19937 rng(31);                          // Kruskal vs Prim on complete graphs
    int bad = 0;
    for (int t = 0; t < 200; ++t) {
        const int n = 2 + int(rng() % 25);
        std::vector<std::vector<ll>> w(n, std::vector<ll>(n, 0));
        std::vector<std::tuple<ll, int, int>> es;
        for (int a = 0; a < n; ++a)
            for (int b = a + 1; b < n; ++b) {
                w[a][b] = w[b][a] = ll(rng() % 100);
                es.emplace_back(w[a][b], a, b);
            }
        int used = 0;
        bad += kruskal(n, es, used) != prim_dense(w) || used != n - 1;
    }
    std::printf("MST cross-check: %d mismatches in 200 graphs\n", bad);
}
