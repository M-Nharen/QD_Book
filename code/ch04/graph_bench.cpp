// The nanosecond penalty of Chapter 4's structures (4.9). Machine-dependent: see the text.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <numeric>
#include <queue>
#include <set>
#include <vector>
using ll = long long;
using Clock = std::chrono::steady_clock;
static std::uint64_t x = 88172645463325252ULL;
static std::uint64_t next() { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return x; }
template <class F> double ms(F f) {
    const auto t0 = Clock::now();
    f();
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}
struct CSR { std::vector<int> off, to, w; };
CSR build(int n, const std::vector<int>& a, const std::vector<int>& b,
          const std::vector<int>& w) {
    CSR g{std::vector<int>(n + 1, 0), std::vector<int>(a.size()),
          std::vector<int>(a.size())};
    for (int v : a) ++g.off[v + 1];
    for (int v = 0; v < n; ++v) g.off[v + 1] += g.off[v];
    std::vector<int> pos(g.off.begin(), g.off.end() - 1);
    for (std::size_t e = 0; e < a.size(); ++e) {
        g.to[pos[a[e]]] = b[e];
        g.w[pos[a[e]]++] = w[e];
    }
    return g;
}
std::vector<int> bfs_csr(const CSR& g, int s) {    // returns the visiting order
    std::vector<int> d(g.off.size() - 1, -1), q{s};
    d[s] = 0;
    for (std::size_t h = 0; h < q.size(); ++h)
        for (int i = g.off[q[h]]; i < g.off[q[h] + 1]; ++i)
            if (d[g.to[i]] < 0) { d[g.to[i]] = d[q[h]] + 1; q.push_back(g.to[i]); }
    return q;
}

int main() {
    // ---- 1) BFS over 2^20 vertices and 2^23 random directed edges, three layouts --------
    const int n = 1 << 20, m = 8 * n;
    std::vector<int> a(m), b(m), w(m);
    for (int e = 0; e < m; ++e) {
        a[e] = int(next() % n); b[e] = int(next() % n); w[e] = int(1 + next() % 1000);
    }
    std::vector<std::vector<int>> adj(n);          // one heap block per vertex
    for (int e = 0; e < m; ++e) adj[a[e]].push_back(b[e]);
    const CSR g = build(n, a, b, w);
    std::size_t seen = 0;
    const double t_vv = ms([&] {
        std::vector<int> d(n, -1), q{0};
        d[0] = 0;
        for (std::size_t h = 0; h < q.size(); ++h)
            for (int u : adj[q[h]])
                if (d[u] < 0) { d[u] = d[q[h]] + 1; q.push_back(u); }
        seen = q.size();
    });
    std::vector<int> order;
    const double t_csr = ms([&] { order = bfs_csr(g, 0); });
    std::vector<int> rank(n, -1);                  // relabel: BFS visiting order first,
    int next_label = 0;                            // then the unreached vertices
    for (int v : order) rank[v] = next_label++;
    for (int v = 0; v < n; ++v)
        if (rank[v] < 0) rank[v] = next_label++;
    std::vector<int> ra(m), rb(m);
    for (int e = 0; e < m; ++e) { ra[e] = rank[a[e]]; rb[e] = rank[b[e]]; }
    const CSR gr = build(n, ra, rb, w);
    const double t_rel = ms([&] { order = bfs_csr(gr, 0); });
    std::printf("1) BFS, %zu vertices reached, ns per edge:\n", seen);
    std::printf("   vector<vector<int>>     %5.2f\n", t_vv * 1e6 / m);
    std::printf("   CSR                     %5.2f\n", t_csr * 1e6 / m);
    std::printf("   CSR, relabelled by BFS  %5.2f\n", t_rel * 1e6 / m);

    // ---- 2) Dijkstra frontier: std::set with erase vs binary heap with stale entries ----
    const int dn = 200'000, dm = 1'000'000;
    std::vector<int> da(dm), db(dm), dw(dm);
    for (int e = 0; e < dm; ++e) {
        da[e] = int(next() % dn); db[e] = int(next() % dn); dw[e] = int(1 + next() % 1000);
    }
    const CSR dg = build(dn, da, db, dw);
    const ll INF = 1LL << 60;
    ll check1 = 0, check2 = 0;
    const double t_set = ms([&] {
        std::vector<ll> d(dn, INF);
        std::set<std::pair<ll, int>> fr{{0, 0}};
        d[0] = 0;
        while (!fr.empty()) {
            const auto [dv, v] = *fr.begin();
            fr.erase(fr.begin());
            for (int i = dg.off[v]; i < dg.off[v + 1]; ++i)
                if (dv + dg.w[i] < d[dg.to[i]]) {
                    if (d[dg.to[i]] < INF) fr.erase({d[dg.to[i]], dg.to[i]});
                    d[dg.to[i]] = dv + dg.w[i];
                    fr.insert({d[dg.to[i]], dg.to[i]});
                }
        }
        for (ll v : d) check1 += v < INF ? v : 0;
    });
    const double t_heap = ms([&] {
        std::vector<ll> d(dn, INF);
        using It = std::pair<ll, int>;
        std::priority_queue<It, std::vector<It>, std::greater<It>> pq;
        d[0] = 0;
        pq.push({0, 0});
        while (!pq.empty()) {
            const auto [dv, v] = pq.top();
            pq.pop();
            if (dv != d[v]) continue;
            for (int i = dg.off[v]; i < dg.off[v + 1]; ++i)
                if (dv + dg.w[i] < d[dg.to[i]]) {
                    d[dg.to[i]] = dv + dg.w[i];
                    pq.push({d[dg.to[i]], dg.to[i]});
                }
        }
        for (ll v : d) check2 += v < INF ? v : 0;
    });
    std::printf("2) Dijkstra on CSR, n=2*10^5, m=10^6 (checksums %s):\n",
                check1 == check2 ? "equal" : "DIFFER");
    std::printf("   std::set + erase        %6.1f ms\n", t_set);
    std::printf("   heap + stale entries    %6.1f ms\n", t_heap);

    // ---- 3) DSU find: a chain of 10^5 vs path compression + union by size ---------------
    const int N = 100'000;
    std::vector<int> par(N);
    std::iota(par.begin(), par.end(), 0);
    for (int i = 1; i < N; ++i) par[i] = i - 1;    // worst case without heuristics: a path
    ll sink = 0;
    const double t_chain = ms([&] {
        for (int q = 0; q < 1000; ++q) {
            int v = N - 1 - q;
            while (par[v] != v) v = par[v];
            sink += v;
        }
    });
    std::vector<int> p2(N), sz(N, 1);
    std::iota(p2.begin(), p2.end(), 0);
    auto find = [&](int v) {
        while (p2[v] != v) { p2[v] = p2[p2[v]]; v = p2[v]; }
        return v;
    };
    for (int i = 1; i < N; ++i) {                  // the same unions, with both heuristics
        int r1 = find(i), r2 = find(i - 1);
        if (sz[r1] < sz[r2]) std::swap(r1, r2);
        p2[r2] = r1;
        sz[r1] += sz[r2];
    }
    const double t_fast = ms([&] {
        for (int q = 0; q < 1'000'000; ++q) sink += find(int(next() % N));
    });
    std::printf("3) DSU find, ns per query: chain %.0f, union by size + halving %.1f\n",
                t_chain * 1e6 / 1000, t_fast * 1e6 / 1'000'000);

    // ---- 4) prefix sums over 2^20 values: Fenwick query vs iterative segment tree -------
    const int F = 1 << 20, Q = 1 << 21;
    std::vector<ll> fen(F + 1, 0), seg(2 * F, 0);
    for (int i = 0; i < F; ++i) {
        const ll v = ll(next() % 100);
        for (int j = i + 1; j <= F; j += j & -j) fen[j] += v;
        seg[F + i] = v;
    }
    for (int i = F - 1; i > 0; --i) seg[i] = seg[2 * i] + seg[2 * i + 1];
    std::vector<int> qs(Q);
    for (int& v : qs) v = int(next() % F);
    ll s1 = 0, s2 = 0;
    const double t_fen = ms([&] {
        for (int r : qs)
            for (int j = r; j > 0; j -= j & -j) s1 += fen[j];
    });
    const double t_seg = ms([&] {
        for (int r : qs)
            for (int l = F, rr = r + F; l < rr; l /= 2, rr /= 2) {
                if (l & 1) s2 += seg[l++];
                if (rr & 1) s2 += seg[--rr];
            }
    });
    std::printf("4) prefix-sum query over 2^20 values (%s): Fenwick %.1f ns, segment tree "
                "%.1f ns\n", s1 == s2 ? "same sums" : "DIFFER", t_fen * 1e6 / Q,
                t_seg * 1e6 / Q);
    std::printf("(sink %lld)\n", sink % 10);
}
