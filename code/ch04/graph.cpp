// Compressed sparse row (CSR) graphs, BFS, 0-1 BFS and iterative cycle detection (4.2).
#include <algorithm>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

struct Edge { int from, to, w; };

// CSR: the out-edges of v are to[off[v]] .. to[off[v+1]-1], all in one array.
struct CSR {
    std::vector<int> off;                          // n + 1 offsets
    std::vector<int> to, w;                        // m targets and weights, by source
    CSR(int n, const std::vector<Edge>& es) : off(n + 1, 0), to(es.size()), w(es.size()) {
        for (const Edge& e : es) ++off[e.from + 1];          // 1) count out-degrees
        for (int v = 0; v < n; ++v) off[v + 1] += off[v];    // 2) prefix sums = starts
        std::vector<int> pos(off.begin(), off.end() - 1);    // 3) place each edge
        for (const Edge& e : es) { to[pos[e.from]] = e.to; w[pos[e.from]++] = e.w; }
    }
    int n() const { return int(off.size()) - 1; }
};

std::vector<int> bfs(const CSR& g, int s) {        // unweighted shortest paths
    std::vector<int> d(g.n(), -1), q;
    q.reserve(g.n());
    d[s] = 0;
    q.push_back(s);
    for (std::size_t h = 0; h < q.size(); ++h) {   // a vector used as a queue
        const int v = q[h];
        for (int i = g.off[v]; i < g.off[v + 1]; ++i)
            if (d[g.to[i]] < 0) { d[g.to[i]] = d[v] + 1; q.push_back(g.to[i]); }
    }
    return d;
}

std::vector<int> bfs01(const CSR& g, int s) {      // weights 0 or 1 only
    std::vector<int> d(g.n(), 1 << 30);
    std::deque<int> dq{s};
    d[s] = 0;
    while (!dq.empty()) {
        const int v = dq.front();
        dq.pop_front();
        for (int i = g.off[v]; i < g.off[v + 1]; ++i) {
            const int u = g.to[i], nd = d[v] + g.w[i];
            if (nd < d[u]) {
                d[u] = nd;
                if (g.w[i] == 0) dq.push_front(u); else dq.push_back(u);
            }
        }
    }
    return d;
}

// Directed cycle detection, iterative DFS with colours: 0 new, 1 on stack, 2 finished.
// Returns one cycle (as a vertex list) or an empty vector.
std::vector<int> find_cycle(const CSR& g) {
    const int n = g.n();
    std::vector<int> colour(n, 0), parent(n, -1), it(n);
    for (int s = 0; s < n; ++s) {
        if (colour[s]) continue;
        std::vector<int> st{s};
        colour[s] = 1;
        it[s] = g.off[s];
        while (!st.empty()) {
            const int v = st.back();
            if (it[v] == g.off[v + 1]) { colour[v] = 2; st.pop_back(); continue; }
            const int u = g.to[it[v]++];           // next unexplored edge of v
            if (colour[u] == 0) {
                colour[u] = 1; parent[u] = v; it[u] = g.off[u];
                st.push_back(u);
            } else if (colour[u] == 1) {           // back edge v -> u closes a cycle
                std::vector<int> cyc{u};
                for (int x = v; x != u; x = parent[x]) cyc.push_back(x);
                std::reverse(cyc.begin() + 1, cyc.end());
                return cyc;
            }
        }
    }
    return {};
}

int main() {
    // A 4x6 grid: '.' costs 0 to enter, '#' costs 1 (a wall to break). 4-neighbour moves.
    const std::vector<std::string> grid{"..#...", "#.#.#.", "..##..", "#..#.#"};
    const int R = int(grid.size()), C = int(grid[0].size());
    std::vector<Edge> es;
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c)
            for (auto [dr, dc] : {std::pair{0, 1}, {1, 0}, {0, -1}, {-1, 0}}) {
                const int r2 = r + dr, c2 = c + dc;
                if (r2 < 0 || r2 >= R || c2 < 0 || c2 >= C) continue;
                es.push_back({r * C + c, r2 * C + c2, grid[r2][c2] == '#'});
            }
    const CSR g(R * C, es);
    const int target = 0 * C + 5;                  // top-right corner
    std::printf("top-left to top-right: %d moves (BFS), %d wall(s) broken (0-1 BFS)\n",
                bfs(g, 0)[target], bfs01(g, 0)[target]);

    const CSR dag(5, {{0, 1, 1}, {1, 2, 1}, {0, 3, 1}, {3, 2, 1}, {2, 4, 1}});
    const CSR cyc(5, {{0, 1, 1}, {1, 2, 1}, {2, 3, 1}, {3, 1, 1}, {3, 4, 1}});
    std::printf("cycle in dag: %s; cycle in second graph:",
                find_cycle(dag).empty() ? "none" : "found");
    for (int v : find_cycle(cyc)) std::printf(" %d", v);
    std::printf("\n");
}
