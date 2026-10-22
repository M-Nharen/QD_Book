// Fenwick tree, iterative segment tree, lazy segment tree; checked against arrays (4.6).
#include <algorithm>
#include <climits>
#include <cstdio>
#include <random>
#include <vector>
using ll = long long;

struct Fenwick {                                   // point add, prefix sum; 1-based inside
    std::vector<ll> t;
    explicit Fenwick(int n) : t(n + 1, 0) {}
    void add(int i, ll v) {                        // a[i] += v, i in [0, n)
        for (++i; i < int(t.size()); i += i & -i) t[i] += v;
    }
    ll prefix(int i) const {                       // a[0] + ... + a[i-1]
        ll s = 0;
        for (; i > 0; i -= i & -i) s += t[i];
        return s;
    }
    ll range(int l, int r) const { return prefix(r) - prefix(l); }   // [l, r)
};

struct MinTree {                                   // iterative; leaves at n..2n-1
    int n;
    std::vector<ll> t;
    explicit MinTree(const std::vector<ll>& a) : n(int(a.size())), t(2 * a.size()) {
        std::copy(a.begin(), a.end(), t.begin() + n);
        for (int i = n - 1; i > 0; --i) t[i] = std::min(t[2 * i], t[2 * i + 1]);
    }
    void set(int i, ll v) {
        for (t[i += n] = v; i > 1; i /= 2) t[i / 2] = std::min(t[i], t[i ^ 1]);
    }
    ll query(int l, int r) const {                 // min over [l, r)
        ll res = LLONG_MAX;
        for (l += n, r += n; l < r; l /= 2, r /= 2) {
            if (l & 1) res = std::min(res, t[l++]);   // l is a right child: take it alone
            if (r & 1) res = std::min(res, t[--r]);   // r-1 is a left child: take it alone
        }
        return res;
    }
};

struct LazySum {                                   // range add, range sum; recursive
    int n;
    std::vector<ll> sum, lazy;                     // lazy[v]: pending add for v's children
    explicit LazySum(int n_) : n(n_), sum(4 * n_, 0), lazy(4 * n_, 0) {}
    void apply(int v, int len, ll x) { sum[v] += x * len; lazy[v] += x; }
    void push(int v, int lo, int mid, int hi) {    // hand the pending add to the children
        if (lazy[v] == 0) return;
        apply(2 * v, mid - lo, lazy[v]);
        apply(2 * v + 1, hi - mid, lazy[v]);
        lazy[v] = 0;
    }
    void add(int l, int r, ll x, int v, int lo, int hi) {     // node v covers [lo, hi)
        if (r <= lo || hi <= l) return;
        if (l <= lo && hi <= r) { apply(v, hi - lo, x); return; }   // fully covered: stop
        const int mid = (lo + hi) / 2;
        push(v, lo, mid, hi);
        add(l, r, x, 2 * v, lo, mid);
        add(l, r, x, 2 * v + 1, mid, hi);
        sum[v] = sum[2 * v] + sum[2 * v + 1];
    }
    ll query(int l, int r, int v, int lo, int hi) {
        if (r <= lo || hi <= l) return 0;
        if (l <= lo && hi <= r) return sum[v];
        const int mid = (lo + hi) / 2;
        push(v, lo, mid, hi);
        return query(l, r, 2 * v, lo, mid) + query(l, r, 2 * v + 1, mid, hi);
    }
    void add(int l, int r, ll x) { add(l, r, x, 1, 0, n); }
    ll query(int l, int r) { return query(l, r, 1, 0, n); }
};

ll inversions(const std::vector<int>& a) {         // values are 0..n-1 (a permutation)
    Fenwick f(int(a.size()));
    ll inv = 0;
    for (int i = int(a.size()) - 1; i >= 0; --i) { // count smaller values to the right
        inv += f.prefix(a[i]);
        f.add(a[i], 1);
    }
    return inv;
}

int main() {
    std::printf("inversions of [3,1,2,0]: %lld, of [4,3,2,1,0]: %lld\n",
                inversions({3, 1, 2, 0}), inversions({4, 3, 2, 1, 0}));
    std::mt19937 rng(41);
    int bad = 0;
    for (int t = 0; t < 200; ++t) {
        const int n = 1 + int(rng() % 50);
        std::vector<ll> a(n);
        for (ll& x : a) x = ll(rng() % 100) - 50;
        Fenwick fw(n);
        for (int i = 0; i < n; ++i) fw.add(i, a[i]);
        MinTree mt(a);
        LazySum ls(n);
        for (int i = 0; i < n; ++i) ls.add(i, i + 1, a[i]);
        for (int op = 0; op < 200; ++op) {
            int l = int(rng() % n), r = int(rng() % n);
            if (l > r) std::swap(l, r);
            ++r;                                   // half-open [l, r), non-empty
            const ll x = ll(rng() % 21) - 10;
            switch (rng() % 3) {
            case 0:                                // point update everywhere
                a[l] += x; fw.add(l, x); mt.set(l, a[l]); ls.add(l, l + 1, x); break;
            case 1:                                // range add (lazy tree only)
                for (int i = l; i < r; ++i) a[i] += x;
                ls.add(l, r, x);
                for (int i = l; i < r; ++i) mt.set(i, a[i]);    // keep the others in sync
                for (int i = l; i < r; ++i) fw.add(i, x);
                break;
            default: {
                ll s = 0, m = LLONG_MAX;
                for (int i = l; i < r; ++i) { s += a[i]; m = std::min(m, a[i]); }
                bad += (fw.range(l, r) != s) + (mt.query(l, r) != m);
                bad += ls.query(l, r) != s;
            }
            }
        }
    }
    std::printf("random cross-check: %d mismatches\n", bad);
}
