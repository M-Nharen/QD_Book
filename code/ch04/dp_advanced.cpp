// Bitmask DP, digit DP and tree DP with rerooting, each checked by brute force (4.5).
#include <algorithm>
#include <cstdio>
#include <numeric>
#include <random>
#include <string>
#include <vector>
using ll = long long;
constexpr ll INF = 1LL << 60;

// Held-Karp: f[mask][v] = cheapest path from city 0 visiting exactly `mask`, ending at v.
ll tsp(const std::vector<std::vector<ll>>& d) {
    const int n = int(d.size());
    std::vector<std::vector<ll>> f(1u << n, std::vector<ll>(n, INF));
    f[1][0] = 0;                                   // only city 0 visited, standing at 0
    for (unsigned mask = 1; mask < (1u << n); ++mask)
        for (int v = 0; v < n; ++v) {
            if (f[mask][v] == INF) continue;
            for (int u = 0; u < n; ++u)
                if (!((mask >> u) & 1u)) {
                    ll& g = f[mask | (1u << u)][u];
                    g = std::min(g, f[mask][v] + d[v][u]);
                }
        }
    ll best = INF;
    for (int v = 1; v < n; ++v) best = std::min(best, f[(1u << n) - 1][v] + d[v][0]);
    return n == 1 ? 0 : best;
}
ll tsp_brute(const std::vector<std::vector<ll>>& d) {
    std::vector<int> p(d.size());
    std::iota(p.begin(), p.end(), 0);
    ll best = INF;
    do {                                           // city 0 stays first
        ll c = 0;
        for (std::size_t i = 0; i < p.size(); ++i) c += d[p[i]][p[(i + 1) % p.size()]];
        best = std::min(best, c);
    } while (std::next_permutation(p.begin() + 1, p.end()));
    return best;
}

// Digit DP: how many x in [0, N] have digit sum exactly S? Walk the digits of N from the
// most significant; "tight" means the prefix so far equals N's prefix.
ll count_digit_sum(ll N, int S) {
    const std::string s = std::to_string(N);
    const int L = int(s.size());
    // ways[i][r] = ways to fill the last L-i digits freely (0..9 each) with digit sum r
    std::vector<std::vector<ll>> ways(L + 1, std::vector<ll>(S + 1, 0));
    ways[L][0] = 1;
    for (int i = L - 1; i >= 0; --i)
        for (int r = 0; r <= S; ++r)
            for (int dgt = 0; dgt <= 9 && dgt <= r; ++dgt)
                ways[i][r] += ways[i + 1][r - dgt];
    ll total = 0;
    int used = 0;                                  // digit sum of the tight prefix
    for (int i = 0; i < L && used <= S; ++i) {
        const int lim = s[i] - '0';
        for (int dgt = 0; dgt < lim && used + dgt <= S; ++dgt)   // go below N here
            total += ways[i + 1][S - used - dgt];
        used += lim;                               // or stay tight
    }
    return total + (used == S);                    // N itself
}

// Rerooting: sum of distances from every vertex to all others, in O(n).
std::vector<ll> sum_of_distances(int n, const std::vector<std::pair<int, int>>& edges) {
    std::vector<std::vector<int>> g(n);
    for (auto [a, b] : edges) { g[a].push_back(b); g[b].push_back(a); }
    std::vector<int> order, parent(n, -1), st{0};
    parent[0] = 0;
    while (!st.empty()) {                          // iterative DFS preorder
        const int v = st.back();
        st.pop_back();
        order.push_back(v);
        for (int u : g[v])
            if (u != parent[v]) { parent[u] = v; st.push_back(u); }
    }
    std::vector<ll> sub(n, 1), down(n, 0), ans(n, 0);
    for (int i = n - 1; i > 0; --i) {              // pass 1, children first: subtree data
        const int v = order[i];
        sub[parent[v]] += sub[v];
        down[parent[v]] += down[v] + sub[v];       // every node below v is one step further
    }
    ans[0] = down[0];
    for (int i = 1; i < n; ++i) {                  // pass 2, parents first: move the root
        const int v = order[i];                    // sub[v] nodes get 1 closer, n-sub[v]
        ans[v] = ans[parent[v]] - sub[v] + (n - sub[v]);   // nodes get 1 further
    }
    return ans;
}

int main() {
    const std::vector<std::vector<ll>> d4{
        {0, 10, 15, 20}, {10, 0, 35, 25}, {15, 35, 0, 30}, {20, 25, 30, 0}};
    std::printf("TSP 4 cities: %lld\n", tsp(d4));
    std::printf("digit sum 27 in [0, 10^9]: %lld;  digit sum 100 in [0, 10^18]: %lld\n",
                count_digit_sum(1'000'000'000, 27),
                count_digit_sum(1'000'000'000'000'000'000LL, 100));
    std::printf("sum of distances:");
    for (ll x : sum_of_distances(6, {{0, 1}, {0, 2}, {2, 3}, {2, 4}, {2, 5}}))
        std::printf(" %lld", x);
    std::printf("\n");

    std::mt19937 rng(77);
    int bad = 0;
    for (int t = 0; t < 100; ++t) {                // TSP vs all permutations
        const int n = 1 + int(rng() % 8);
        std::vector<std::vector<ll>> d(n, std::vector<ll>(n, 0));
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) d[i][j] = i == j ? 0 : ll(rng() % 50);
        bad += tsp(d) != tsp_brute(d);
    }
    for (int t = 0; t < 200; ++t) {                // digit DP vs counting
        const ll N = ll(rng() % 100'000);
        const int S = int(rng() % 40);
        ll b = 0;
        for (ll x = 0; x <= N; ++x) {
            int s = 0;
            for (ll y = x; y; y /= 10) s += int(y % 10);
            b += s == S;
        }
        bad += count_digit_sum(N, S) != b;
    }
    for (int t = 0; t < 100; ++t) {                // rerooting vs BFS from every vertex
        const int n = 1 + int(rng() % 40);
        std::vector<std::pair<int, int>> es;
        for (int v = 1; v < n; ++v) es.push_back({int(rng() % unsigned(v)), v});
        const auto got = sum_of_distances(n, es);
        std::vector<std::vector<int>> g(n);
        for (auto [a, b] : es) { g[a].push_back(b); g[b].push_back(a); }
        for (int s = 0; s < n; ++s) {
            std::vector<int> dist(n, -1), q{s};
            dist[s] = 0;
            for (std::size_t h = 0; h < q.size(); ++h)
                for (int u : g[q[h]])
                    if (dist[u] < 0) { dist[u] = dist[q[h]] + 1; q.push_back(u); }
            bad += got[s] != std::accumulate(dist.begin(), dist.end(), 0LL);
        }
    }
    std::printf("random cross-checks: %d mismatches\n", bad);
}
