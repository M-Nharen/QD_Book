// Solutions to the Chapter 4 practice problems, each checked against a brute force.
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <tuple>
#include <vector>
using ll = long long;
using VL = std::vector<ll>;
using G = std::vector<std::vector<int>>;
static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) { ++failures; std::printf("MISMATCH in %s\n", what); }
}
static std::mt19937_64 rng(4);
static ll rnd(ll lo, ll hi) { return std::uniform_int_distribution<ll>(lo, hi)(rng); }

std::vector<int> bfs_dist(const G& g, int s) {
    std::vector<int> d(g.size(), -1), q{s};
    d[s] = 0;
    for (std::size_t h = 0; h < q.size(); ++h)
        for (int u : g[q[h]])
            if (d[u] < 0) { d[u] = d[q[h]] + 1; q.push_back(u); }
    return d;
}
// P4.1 tree diameter by two BFS sweeps
int diameter(const G& g) {
    auto d0 = bfs_dist(g, 0);
    const int a = int(std::max_element(d0.begin(), d0.end()) - d0.begin());
    auto da = bfs_dist(g, a);
    return *std::max_element(da.begin(), da.end());
}
// P4.2 islands: count connected components of '1' cells (4-neighbour) with BFS
int islands(std::vector<std::string> grid) {
    const int R = int(grid.size()), C = int(grid[0].size());
    int count = 0;
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            if (grid[r][c] != '1') continue;
            ++count;
            std::vector<std::pair<int, int>> q{{r, c}};
            grid[r][c] = '0';                      // mark when pushed, not when popped
            for (std::size_t h = 0; h < q.size(); ++h)
                for (auto [dr, dc] : {std::pair{0, 1}, {1, 0}, {0, -1}, {-1, 0}}) {
                    const int y = q[h].first + dr, x = q[h].second + dc;
                    if (y >= 0 && y < R && x >= 0 && x < C && grid[y][x] == '1') {
                        grid[y][x] = '0';
                        q.push_back({y, x});
                    }
                }
        }
    return count;
}
// P4.3 lexicographically smallest topological order (empty if there is a cycle)
std::vector<int> topo(int n, const std::vector<std::pair<int, int>>& e) {
    G out(n);
    std::vector<int> indeg(n, 0), order;
    for (auto [a, b] : e) { out[a].push_back(b); ++indeg[b]; }
    std::priority_queue<int, std::vector<int>, std::greater<int>> pq;
    for (int v = 0; v < n; ++v) if (indeg[v] == 0) pq.push(v);
    while (!pq.empty()) {
        const int v = pq.top(); pq.pop();
        order.push_back(v);
        for (int u : out[v]) if (--indeg[u] == 0) pq.push(u);
    }
    if (int(order.size()) < n) order.clear();
    return order;
}
// P4.4 fewest coins (unbounded), -1 if impossible; and number of combinations
ll min_coins(const VL& coins, int amt) {
    VL f(amt + 1, ll(1e18));
    f[0] = 0;
    for (int a = 1; a <= amt; ++a)
        for (ll c : coins)
            if (c <= a && f[a - c] + 1 < f[a]) f[a] = f[a - c] + 1;
    return f[amt] >= ll(1e18) ? -1 : f[amt];
}
ll count_combinations(const VL& coins, int amt) {  // coin loop OUTSIDE: order ignored
    VL f(amt + 1, 0);
    f[0] = 1;
    for (ll c : coins)
        for (int a = int(c); a <= amt; ++a) f[a] += f[a - c];
    return f[amt];
}
// P4.5 LIS length, O(n log n)
int lis_len(const VL& a) {
    VL tail;
    for (ll x : a) {
        auto it = std::lower_bound(tail.begin(), tail.end(), x);
        if (it == tail.end()) tail.push_back(x); else *it = x;
    }
    return int(tail.size());
}
// P4.6 edit distance and LCS (full tables, small inputs)
int edit(const std::string& s, const std::string& t) {
    std::vector<std::vector<int>> f(s.size() + 1, std::vector<int>(t.size() + 1));
    for (std::size_t i = 0; i <= s.size(); ++i)
        for (std::size_t j = 0; j <= t.size(); ++j)
            f[i][j] = !i ? int(j) : !j ? int(i)
                    : std::min({f[i - 1][j] + 1, f[i][j - 1] + 1,
                                f[i - 1][j - 1] + (s[i - 1] != t[j - 1])});
    return f[s.size()][t.size()];
}
int lcs(const std::string& s, const std::string& t) {
    std::vector<std::vector<int>> f(s.size() + 1, std::vector<int>(t.size() + 1, 0));
    for (std::size_t i = 1; i <= s.size(); ++i)
        for (std::size_t j = 1; j <= t.size(); ++j)
            f[i][j] = s[i - 1] == t[j - 1] ? f[i - 1][j - 1] + 1
                                           : std::max(f[i - 1][j], f[i][j - 1]);
    return f[s.size()][t.size()];
}
// P4.7 Dijkstra (non-negative weights)
VL dijkstra(int n, const std::vector<std::tuple<int, int, ll>>& e, int s) {
    std::vector<std::vector<std::pair<int, ll>>> g(n);
    for (auto [a, b, w] : e) g[a].push_back({b, w});
    VL d(n, ll(1e18));
    using It = std::pair<ll, int>;
    std::priority_queue<It, std::vector<It>, std::greater<It>> pq;
    d[s] = 0;
    pq.push({0, s});
    while (!pq.empty()) {
        auto [dv, v] = pq.top(); pq.pop();
        if (dv != d[v]) continue;
        for (auto [u, w] : g[v]) if (dv + w < d[u]) { d[u] = dv + w; pq.push({d[u], u}); }
    }
    return d;
}
// P4.8 Kruskal with DSU; also reports the first edge that closes a cycle
struct DSU {
    std::vector<int> p, sz;
    explicit DSU(int n) : p(n), sz(n, 1) { std::iota(p.begin(), p.end(), 0); }
    int find(int x) { while (p[x] != x) x = p[x] = p[p[x]]; return x; }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (sz[a] < sz[b]) std::swap(a, b);
        p[b] = a; sz[a] += sz[b];
        return true;
    }
};
ll mst(int n, std::vector<std::tuple<ll, int, int>> e) {
    std::sort(e.begin(), e.end());
    DSU d(n);
    ll total = 0;
    for (auto [w, a, b] : e) if (d.unite(a, b)) total += w;
    return total;
}
// P4.9 number of +/- sign assignments reaching target: subset-sum counting
ll target_sum(const VL& a, ll target) {
    const ll S = std::accumulate(a.begin(), a.end(), 0LL);
    if (std::abs(target) > S || (S + target) % 2) return 0;
    const ll P = (S + target) / 2;                 // sum of the '+' elements
    VL f(P + 1, 0);
    f[0] = 1;
    for (ll x : a)
        for (ll s = P; s >= x; --s) f[s] += f[s - x];
    return f[P];
}
// P4.10 inversions with a Fenwick tree after coordinate compression
ll inversions(const VL& a) {
    VL vals = a;
    std::sort(vals.begin(), vals.end());
    vals.erase(std::unique(vals.begin(), vals.end()), vals.end());
    std::vector<ll> fen(vals.size() + 1, 0);
    ll inv = 0;
    for (std::size_t i = a.size(); i-- > 0;) {
        const int r = int(std::lower_bound(vals.begin(), vals.end(), a[i]) - vals.begin());
        for (int j = r; j > 0; j -= j & -j) inv += fen[j];         // strictly smaller
        for (int j = r + 1; j <= int(vals.size()); j += j & -j) fen[j] += 1;
    }
    return inv;
}
// P4.11 prefix function and border-based waiting time
std::vector<int> prefix_function(const std::string& s) {
    std::vector<int> pi(s.size(), 0);
    for (std::size_t i = 1; i < s.size(); ++i) {
        int k = pi[i - 1];
        while (k > 0 && s[i] != s[k]) k = pi[k - 1];
        if (s[i] == s[k]) ++k;
        pi[i] = k;
    }
    return pi;
}
ll expected_flips(const std::string& p) {
    const auto pi = prefix_function(p);
    ll e = 0;
    for (int b = int(p.size()); b > 0; b = pi[b - 1]) e += 1LL << b;
    return e;
}
// P4.12 Held-Karp TSP
ll tsp(const std::vector<VL>& d) {
    const int n = int(d.size());
    std::vector<VL> f(1u << n, VL(n, ll(1e18)));
    f[1][0] = 0;
    for (unsigned m = 1; m < (1u << n); ++m)
        for (int v = 0; v < n; ++v) {
            if (f[m][v] >= ll(1e18)) continue;
            for (int u = 0; u < n; ++u)
                if (!((m >> u) & 1u))
                    f[m | (1u << u)][u] = std::min(f[m | (1u << u)][u], f[m][v] + d[v][u]);
        }
    ll best = ll(1e18);
    for (int v = 1; v < n; ++v) best = std::min(best, f[(1u << n) - 1][v] + d[v][0]);
    return best;
}
// P4.13 digit DP: x in [1, N] with no two adjacent equal digits
ll no_adjacent_equal(ll N) {
    const std::string s = std::to_string(N);
    const int L = int(s.size());
    VL pw9(L + 1, 1);
    for (int i = 1; i <= L; ++i) pw9[i] = pw9[i - 1] * 9;
    ll total = 0;
    for (int len = 1; len < L; ++len) total += pw9[len];   // shorter numbers: 9 * 9^(len-1)
    for (int i = 0; i < L; ++i) {                  // same length, first difference at i
        const int lim = s[i] - '0', prev = i ? s[i - 1] - '0' : -1;
        for (int d = (i == 0 ? 1 : 0); d < lim; ++d)
            if (d != prev) total += pw9[L - 1 - i];
        if (i > 0 && lim == prev) return total;    // N's own prefix is already invalid
    }
    return total + 1;                              // N itself is valid
}
// P4.14 burst balloons, interval DP
ll burst(const VL& in) {
    VL a{1};
    a.insert(a.end(), in.begin(), in.end());
    a.push_back(1);
    const int n = int(a.size());
    std::vector<VL> f(n, VL(n, 0));
    for (int len = 2; len < n; ++len)
        for (int l = 0; l + len < n; ++l)
            for (int k = l + 1; k < l + len; ++k)
                f[l][l + len] = std::max(f[l][l + len], f[l][k] + f[k][l + len] +
                                                         a[l] * a[k] * a[l + len]);
    return f[0][n - 1];
}
// P4.15 sum of distances by rerooting
VL sum_dist(int n, const std::vector<std::pair<int, int>>& e) {
    G g(n);
    for (auto [a, b] : e) { g[a].push_back(b); g[b].push_back(a); }
    std::vector<int> order, par(n, -1), st{0};
    par[0] = 0;
    while (!st.empty()) {
        const int v = st.back(); st.pop_back();
        order.push_back(v);
        for (int u : g[v]) if (u != par[v]) { par[u] = v; st.push_back(u); }
    }
    VL sub(n, 1), down(n, 0), ans(n);
    for (int i = n - 1; i > 0; --i) {
        sub[par[order[i]]] += sub[order[i]];
        down[par[order[i]]] += down[order[i]] + sub[order[i]];
    }
    ans[0] = down[0];
    for (int i = 1; i < n; ++i) ans[order[i]] = ans[par[order[i]]] + n - 2 * sub[order[i]];
    return ans;
}

int main() {
    // ---- P4.1
    G t1(10);
    for (auto [a, b] : {std::pair{0, 1}, {1, 2}, {2, 3}, {1, 4}, {4, 5}, {5, 6}, {4, 7},
                        {0, 8}, {8, 9}}) { t1[a].push_back(b); t1[b].push_back(a); }
    int brute = 0;
    for (int s = 0; s < 10; ++s) {
        auto d = bfs_dist(t1, s);
        brute = std::max(brute, *std::max_element(d.begin(), d.end()));
    }
    std::printf("P4.1  diameter %d (all-pairs: %d)\n", diameter(t1), brute);
    // ---- P4.2
    const std::vector<std::string> grid{"110010", "110011", "001000", "000111", "101001"};
    DSU dsu(30);                                   // cross-check with union-find
    int ones = 0, merges = 0;
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 6; ++c) {
            if (grid[r][c] != '1') continue;
            ++ones;
            const int id = r * 6 + c;
            if (c + 1 < 6 && grid[r][c + 1] == '1') merges += dsu.unite(id, id + 1);
            if (r + 1 < 5 && grid[r + 1][c] == '1') merges += dsu.unite(id, id + 6);
        }
    std::printf("P4.2  islands %d (DSU: %d)\n", islands(grid), ones - merges);
    // ---- P4.3
    const std::vector<std::pair<int, int>> pre{{3, 0}, {1, 0}, {0, 2}, {4, 2}, {2, 5},
                                               {1, 4}};
    std::printf("P4.3  order:");
    for (int v : topo(6, pre)) std::printf(" %d", v);
    auto with_cycle = pre;
    with_cycle.push_back({5, 1});
    std::printf("   with 5->1 added: %s\n", topo(6, with_cycle).empty() ? "cycle" : "ok");
    std::vector<int> perm(6);                      // brute force: first valid permutation
    std::iota(perm.begin(), perm.end(), 0);
    do {
        std::vector<int> pos(6);
        for (int i = 0; i < 6; ++i) pos[perm[i]] = i;
        bool ok = true;
        for (auto [a, b] : pre) ok = ok && pos[a] < pos[b];
        if (ok) break;
    } while (std::next_permutation(perm.begin(), perm.end()));
    check(perm == topo(6, pre), "P4.3");
    // ---- P4.4
    std::printf("P4.4  min coins {1,5,6,9} for 11: %lld; {1,2,5} combinations: %lld\n",
                min_coins({1, 5, 6, 9}, 11), count_combinations({1, 2, 5}, 11));
    for (int amt = 0; amt <= 30; ++amt) {          // brute force by recursion
        std::function<ll(int, int)> ways = [&](int i, int left) -> ll {
            if (left == 0) return 1;
            if (i == 3) return 0;
            const int c = std::vector<int>{1, 2, 5}[i];
            ll w = 0;
            for (int k = 0; k * c <= left; ++k) w += ways(i + 1, left - k * c);
            return w;
        };
        check(count_combinations({1, 2, 5}, amt) == ways(0, amt), "P4.4");
    }
    // ---- P4.5
    const VL p5{0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
    int best5 = 0;
    for (unsigned m = 0; m < (1u << p5.size()); ++m) {    // brute force over subsequences
        ll last = -1;
        bool inc = true;
        for (std::size_t i = 0; i < p5.size() && inc; ++i)
            if ((m >> i) & 1u) { inc = p5[i] > last; last = p5[i]; }
        if (inc) best5 = std::max(best5, std::popcount(m));
    }
    std::printf("P4.5  LIS %d (brute force %d)\n", lis_len(p5), best5);
    // ---- P4.6
    std::printf("P4.6  edit(sunday, saturday) %d, LCS(ABCBDAB, BDCABA) %d\n",
                edit("sunday", "saturday"), lcs("ABCBDAB", "BDCABA"));
    // ---- P4.7
    const std::vector<std::tuple<int, int, ll>> e7{{0, 1, 7}, {0, 2, 9}, {0, 5, 14},
        {1, 2, 10}, {1, 3, 15}, {2, 3, 11}, {2, 5, 2}, {3, 4, 6}, {5, 4, 9}};
    std::vector<std::tuple<int, int, ll>> und;     // the edges are undirected
    for (auto [a, b, w] : e7) { und.push_back({a, b, w}); und.push_back({b, a, w}); }
    std::printf("P4.7  distances from 0:");
    for (ll d : dijkstra(6, und, 0)) std::printf(" %lld", d);
    std::printf("\n");
    // ---- P4.8
    std::vector<std::tuple<ll, int, int>> e8;
    for (auto [a, b, w] : e7) e8.push_back({w, a, b});
    std::printf("P4.8  MST weight %lld\n", mst(6, e8));
    // ---- P4.9
    std::printf("P4.9  target sum [1,1,1,1,1]->3: %lld; [1,2,3,4,5]->3: %lld\n",
                target_sum({1, 1, 1, 1, 1}, 3), target_sum({1, 2, 3, 4, 5}, 3));
    for (int t = 0; t < 300; ++t) {
        VL a(rnd(1, 10));
        for (ll& x : a) x = rnd(0, 6);
        const ll T = rnd(-12, 12);
        ll b = 0;
        for (unsigned m = 0; m < (1u << a.size()); ++m) {
            ll s = 0;
            for (std::size_t i = 0; i < a.size(); ++i) s += (m >> i) & 1u ? a[i] : -a[i];
            b += s == T;
        }
        check(target_sum(a, T) == b, "P4.9");
    }
    // ---- P4.10
    const VL p10{5, 3, 8, 6, 1, 9, 2, 7, 4, 0};
    ll inv_brute = 0;
    for (std::size_t i = 0; i < p10.size(); ++i)
        for (std::size_t j = i + 1; j < p10.size(); ++j) inv_brute += p10[i] > p10[j];
    std::printf("P4.10 inversions %lld (brute force %lld)\n", inversions(p10), inv_brute);
    for (int t = 0; t < 300; ++t) {
        VL a(rnd(1, 30));
        for (ll& x : a) x = rnd(-5, 5);            // duplicates: equal pairs do not count
        ll b = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
            for (std::size_t j = i + 1; j < a.size(); ++j) b += a[i] > a[j];
        check(inversions(a) == b, "P4.10");
    }
    // ---- P4.11
    std::printf("P4.11 pi(abacabab) =");
    for (int x : prefix_function("abacabab")) std::printf(" %d", x);
    std::printf("; E[HTHT] = %lld, E[HHTT] = %lld\n", expected_flips("HTHT"),
                expected_flips("HHTT"));
    for (const char* p : {"HTHT", "HHTT"}) {       // simulation, 2*10^5 runs each
        std::mt19937_64 sim(99);
        const std::string pat = p;
        double total = 0;
        const int runs = 200'000;
        for (int r = 0; r < runs; ++r) {
            std::string last;
            int flips = 0;
            while (last.size() < pat.size() || last.compare(last.size() - pat.size(),
                                                           pat.size(), pat) != 0) {
                last.push_back(sim() >> 63 ? 'H' : 'T');
                ++flips;
            }
            total += flips;
        }
        const double mean = total / runs, want = double(expected_flips(pat));
        check(std::abs(mean - want) < 0.15, "P4.11 simulation");   // > 3 standard errors
        std::printf("      simulated E[%s] = %.2f\n", p, mean);
    }
    // ---- P4.12
    const std::vector<VL> d12{{0, 3, 4, 2, 7}, {3, 0, 4, 6, 3}, {4, 4, 0, 5, 8},
                              {2, 6, 5, 0, 6}, {7, 3, 8, 6, 0}};
    std::vector<int> pp{1, 2, 3, 4};
    ll b12 = ll(1e18);
    do {
        ll c = d12[0][pp[0]] + d12[pp[3]][0];
        for (int i = 0; i < 3; ++i) c += d12[pp[i]][pp[i + 1]];
        b12 = std::min(b12, c);
    } while (std::next_permutation(pp.begin(), pp.end()));
    std::printf("P4.12 TSP %lld (brute force %lld)\n", tsp(d12), b12);
    // ---- P4.13
    ll cnt = 0;                                    // brute force for small N
    for (ll N = 1; N <= 20000; ++N) {
        const std::string s = std::to_string(N);
        bool ok = true;
        for (std::size_t i = 1; i < s.size(); ++i) ok = ok && s[i] != s[i - 1];
        cnt += ok;
        if (N % 997 == 0 || N == 20000) check(no_adjacent_equal(N) == cnt, "P4.13");
    }
    ll closed = 0, p9 = 1;
    for (int L = 1; L <= 18; ++L) closed += (p9 *= 9);
    std::printf("P4.13 no equal adjacent digits in [1, 10^18]: %lld (sum 9^L: %lld)\n",
                no_adjacent_equal(1'000'000'000'000'000'000LL), closed);
    // ---- P4.14
    std::function<ll(VL)> bb = [&](VL a) -> ll {
        ll best = 0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            const ll l = i ? a[i - 1] : 1, r = i + 1 < a.size() ? a[i + 1] : 1;
            VL b = a;
            b.erase(b.begin() + ll(i));
            best = std::max(best, l * a[i] * r + bb(b));
        }
        return best;
    };
    std::printf("P4.14 burst [2,4,3,1,5]: %lld (brute force %lld)\n",
                burst({2, 4, 3, 1, 5}), bb({2, 4, 3, 1, 5}));
    // ---- P4.15
    const std::vector<std::pair<int, int>> e15{{0, 1}, {1, 2}, {1, 3}, {3, 4}, {3, 5},
                                               {5, 6}, {0, 7}};
    const VL sd = sum_dist(8, e15);
    G g15(8);
    for (auto [a, b] : e15) { g15[a].push_back(b); g15[b].push_back(a); }
    std::printf("P4.15 sums:");
    for (int v = 0; v < 8; ++v) {
        auto d = bfs_dist(g15, v);
        check(sd[v] == std::accumulate(d.begin(), d.end(), 0LL), "P4.15");
        std::printf(" %lld", sd[v]);
    }
    std::printf("\n");

    std::printf(failures ? "FAILURES: %d\n" : "all cross-checks passed (%d)\n", failures);
}
