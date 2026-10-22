// Rooted trees without recursion: DFS order, subtree sizes, diameter, LCA (4.1).
#include <algorithm>
#include <cstdio>
#include <random>
#include <utility>
#include <vector>
using Graph = std::vector<std::vector<int>>;

struct RootedTree {
    int n, LOG;
    std::vector<int> parent, depth, order, size;   // order: DFS preorder from the root
    std::vector<std::vector<int>> up;              // up[k][v] = 2^k-th ancestor of v

    RootedTree(const Graph& g, int root) : n(int(g.size())), LOG(1) {
        while ((1 << LOG) < n) ++LOG;
        parent.assign(n, -1); depth.assign(n, 0); size.assign(n, 1);
        std::vector<int> stack{root};              // explicit stack: no depth limit
        parent[root] = root;
        while (!stack.empty()) {
            const int v = stack.back();
            stack.pop_back();
            order.push_back(v);
            for (int w : g[v])
                if (w != parent[v]) {
                    parent[w] = v;
                    depth[w] = depth[v] + 1;
                    stack.push_back(w);
                }
        }
        for (int i = n - 1; i > 0; --i)            // reverse preorder: children first
            size[parent[order[i]]] += size[order[i]];
        up.assign(LOG, std::vector<int>(n));
        up[0] = parent;                            // the root is its own parent
        for (int k = 1; k < LOG; ++k)
            for (int v = 0; v < n; ++v) up[k][v] = up[k - 1][up[k - 1][v]];
    }
    int lca(int a, int b) const {
        if (depth[a] < depth[b]) std::swap(a, b);
        for (int k = LOG - 1; k >= 0; --k)         // lift a to b's depth, bit by bit
            if (depth[a] - (1 << k) >= depth[b]) a = up[k][a];
        if (a == b) return a;
        for (int k = LOG - 1; k >= 0; --k)         // lift both while they stay distinct
            if (up[k][a] != up[k][b]) { a = up[k][a]; b = up[k][b]; }
        return parent[a];
    }
    int dist(int a, int b) const { return depth[a] + depth[b] - 2 * depth[lca(a, b)]; }
};

std::pair<int, int> farthest(const Graph& g, int s) {        // BFS: (vertex, distance)
    std::vector<int> d(g.size(), -1), q{s};
    d[s] = 0;
    for (std::size_t h = 0; h < q.size(); ++h)
        for (int w : g[q[h]])
            if (d[w] < 0) { d[w] = d[q[h]] + 1; q.push_back(w); }
    const int far = q.back();                      // BFS visits in distance order
    return {far, d[far]};
}

int diameter(const Graph& g) {                     // two sweeps (valid for trees only)
    return farthest(g, farthest(g, 0).first).second;
}

Graph random_tree(int n, std::mt19937& rng) {
    Graph g(n);
    for (int v = 1; v < n; ++v) {
        const int p = int(rng() % unsigned(v));    // attach to an earlier vertex
        g[v].push_back(p);
        g[p].push_back(v);
    }
    return g;
}

int main() {
    //        0
    //      / | \       example tree; the longest path is 6-4-1-0-2-5-7 (6 edges)
    //     1  2  3
    //     |  |
    //     4  5
    //     |  |
    //     6  7
    Graph g(8);
    for (auto [a, b] : {std::pair{0, 1}, {0, 2}, {0, 3}, {1, 4}, {2, 5}, {4, 6}, {5, 7}}) {
        g[a].push_back(b);
        g[b].push_back(a);
    }
    RootedTree t(g, 0);
    std::printf("diameter %d, lca(6,7)=%d, lca(6,1)=%d, dist(6,3)=%d, size(2)=%d\n",
                diameter(g), t.lca(6, 7), t.lca(6, 1), t.dist(6, 3), t.size[2]);

    std::mt19937 rng(17);                          // random trees vs a naive LCA
    int bad = 0;
    for (int rep = 0; rep < 20; ++rep) {
        const Graph h = random_tree(500, rng);
        RootedTree rt(h, 0);
        for (int q = 0; q < 500; ++q) {
            int a = int(rng() % 500), b = int(rng() % 500);
            const int got = rt.lca(a, b);
            while (a != b) {                       // naive: step the deeper one upward
                if (rt.depth[a] < rt.depth[b]) std::swap(a, b);
                a = rt.parent[a];
            }
            bad += got != a;
        }
    }
    std::printf("LCA cross-check: %d mismatches in 10000 queries\n", bad);

    Graph path(1'000'000);                         // a path: 10^6 levels deep
    for (int v = 1; v < 1'000'000; ++v) {
        path[v].push_back(v - 1);
        path[v - 1].push_back(v);
    }
    RootedTree deep(path, 0);
    std::printf("path of 10^6 vertices: depth of last = %d, diameter = %d\n",
                deep.depth[999'999], diameter(path));
}
