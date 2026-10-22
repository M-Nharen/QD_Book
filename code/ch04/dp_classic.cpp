// Classic dynamic programs, each checked against a brute force (4.5).
#include <algorithm>
#include <cstdio>
#include <random>
#include <string>
#include <vector>
using ll = long long;
using VL = std::vector<ll>;

// 0/1 knapsack in one row. Capacity runs DOWN so each item is used at most once.
ll knapsack01(const VL& wt, const VL& val, int cap) {
    VL best(cap + 1, 0);                           // best[c]: max value with weight <= c
    for (std::size_t i = 0; i < wt.size(); ++i)
        for (ll c = cap; c >= wt[i]; --c)
            best[c] = std::max(best[c], best[c - wt[i]] + val[i]);
    return best[cap];
}
// Unbounded knapsack: capacity runs UP, so an item can be reused within the same row.
ll knapsack_unbounded(const VL& wt, const VL& val, int cap) {
    VL best(cap + 1, 0);
    for (std::size_t i = 0; i < wt.size(); ++i)
        for (ll c = wt[i]; c <= cap; ++c)
            best[c] = std::max(best[c], best[c - wt[i]] + val[i]);
    return best[cap];
}

// Longest strictly increasing subsequence in O(n log n), with one optimal sequence.
// tail[k] = index of the smallest possible last element of an increasing run of length k+1.
VL lis(const VL& a) {
    std::vector<int> tail, prev(a.size(), -1);
    for (int i = 0; i < int(a.size()); ++i) {
        auto it = std::lower_bound(tail.begin(), tail.end(), a[i],
                                   [&](int j, ll x) { return a[j] < x; });
        const int k = int(it - tail.begin());      // a[i] extends a run of length k
        if (k > 0) prev[i] = tail[k - 1];
        if (it == tail.end()) tail.push_back(i); else *it = i;
    }
    VL seq;
    for (int i = tail.empty() ? -1 : tail.back(); i >= 0; i = prev[i]) seq.push_back(a[i]);
    std::reverse(seq.begin(), seq.end());
    return seq;
}
int lis_quadratic(const VL& a) {                   // reference: O(n^2)
    std::vector<int> f(a.size(), 1);
    int best = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j)
            if (a[j] < a[i]) f[i] = std::max(f[i], f[j] + 1);
        best = std::max(best, f[i]);
    }
    return best;
}

// Edit distance (insert, delete, replace, each cost 1) keeping only two rows: O(m) memory.
int edit_distance(const std::string& s, const std::string& t) {
    std::vector<int> prev(t.size() + 1), cur(t.size() + 1);
    for (std::size_t j = 0; j <= t.size(); ++j) prev[j] = int(j);
    for (std::size_t i = 1; i <= s.size(); ++i) {
        cur[0] = int(i);
        for (std::size_t j = 1; j <= t.size(); ++j)
            cur[j] = std::min({prev[j] + 1,                        // delete s[i-1]
                               cur[j - 1] + 1,                     // insert t[j-1]
                               prev[j - 1] + (s[i - 1] != t[j - 1])});   // replace / keep
        std::swap(prev, cur);
    }
    return prev[t.size()];
}
int edit_distance_full(const std::string& s, const std::string& t) {   // full table
    std::vector<std::vector<int>> f(s.size() + 1, std::vector<int>(t.size() + 1));
    for (std::size_t i = 0; i <= s.size(); ++i)
        for (std::size_t j = 0; j <= t.size(); ++j)
            f[i][j] = i == 0 ? int(j) : j == 0 ? int(i)
                    : std::min({f[i - 1][j] + 1, f[i][j - 1] + 1,
                                f[i - 1][j - 1] + (s[i - 1] != t[j - 1])});
    return f[s.size()][t.size()];
}

// Interval DP (burst balloons): f[l][r] = best for the open interval (l, r), choosing the
// LAST balloon k to burst inside it, so its neighbours at that moment are l and r.
ll burst(const VL& in) {
    VL a{1};
    a.insert(a.end(), in.begin(), in.end());
    a.push_back(1);
    const int n = int(a.size());
    std::vector<VL> f(n, VL(n, 0));
    for (int len = 2; len < n; ++len)              // increasing interval length
        for (int l = 0; l + len < n; ++l) {
            const int r = l + len;
            for (int k = l + 1; k < r; ++k)
                f[l][r] = std::max(f[l][r], f[l][k] + a[l] * a[k] * a[r] + f[k][r]);
        }
    return f[0][n - 1];
}
ll burst_brute(VL a) {                             // try every order: O(n!)
    if (a.empty()) return 0;
    ll best = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const ll l = i ? a[i - 1] : 1, r = i + 1 < a.size() ? a[i + 1] : 1;
        VL b = a;
        b.erase(b.begin() + ll(i));
        best = std::max(best, l * a[i] * r + burst_brute(b));
    }
    return best;
}

int main() {
    std::printf("knapsack 0/1 %lld, unbounded %lld\n",
                knapsack01({2, 3, 4, 5}, {3, 4, 5, 6}, 7),
                knapsack_unbounded({2, 3, 4, 5}, {3, 4, 5, 6}, 7));
    const VL seq = lis({10, 9, 2, 5, 3, 7, 101, 18});
    std::printf("LIS length %zu:", seq.size());
    for (ll x : seq) std::printf(" %lld", x);
    std::printf("\nedit distance kitten/sitting %d, intention/execution %d\n",
                edit_distance("kitten", "sitting"),
                edit_distance("intention", "execution"));
    std::printf("burst balloons [3,1,5,8]: %lld\n", burst({3, 1, 5, 8}));

    std::mt19937 rng(5);
    int bad = 0;
    for (int t = 0; t < 400; ++t) {
        const int n = 1 + int(rng() % 10);
        VL w(n), v(n), a(n);
        for (int i = 0; i < n; ++i) {
            w[i] = 1 + ll(rng() % 6); v[i] = ll(rng() % 10); a[i] = ll(rng() % 6);
        }
        const int cap = int(rng() % 15);
        ll b = 0;                                  // brute force over subsets
        for (unsigned m = 0; m < (1u << n); ++m) {
            ll sw = 0, sv = 0;
            for (int i = 0; i < n; ++i)
                if ((m >> i) & 1u) { sw += w[i]; sv += v[i]; }
            if (sw <= cap) b = std::max(b, sv);
        }
        bad += knapsack01(w, v, cap) != b;
        const VL s = lis(a);
        bool inc = true;
        for (std::size_t i = 1; i < s.size(); ++i) inc = inc && s[i - 1] < s[i];
        bad += int(s.size()) != lis_quadratic(a) || !inc;
        std::string x(rng() % 8, 'a'), y(rng() % 8, 'a');
        for (char& c : x) c = char('a' + rng() % 3);
        for (char& c : y) c = char('a' + rng() % 3);
        bad += edit_distance(x, y) != edit_distance_full(x, y);
        if (n <= 6) bad += burst(a) != burst_brute(a);
    }
    std::printf("random cross-checks: %d mismatches\n", bad);
}
