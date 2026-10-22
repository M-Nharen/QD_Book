// Solutions to the Chapter 3 practice problems, each checked against a brute force.
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
using ll = long long;
using VL = std::vector<ll>;
static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) { ++failures; std::printf("MISMATCH in %s\n", what); }
}
static std::mt19937_64 rng(3);
static ll rnd(ll lo, ll hi) { return std::uniform_int_distribution<ll>(lo, hi)(rng); }

// P3.2 difference array: updates (l, r, v) on 1-indexed [l, r]
VL apply_updates(int n, const std::vector<std::array<ll, 3>>& ups) {
    VL d(n + 2, 0), a(n + 1, 0);
    for (auto [l, r, v] : ups) { d[l] += v; d[r + 1] -= v; }
    for (int i = 1; i <= n; ++i) a[i] = a[i - 1] + d[i];
    return VL(a.begin() + 1, a.end());
}
// P3.3 subarrays with sum exactly k: count earlier prefixes equal to prefix - k
ll count_sum_k(const VL& a, ll k) {
    std::unordered_map<ll, ll> seen{{0, 1}};       // the empty prefix
    ll pre = 0, cnt = 0;
    for (ll x : a) {
        pre += x;
        if (auto it = seen.find(pre - k); it != seen.end()) cnt += it->second;
        ++seen[pre];
    }
    return cnt;
}
// P3.4 least eating speed: first true of "finishes within h hours"
ll min_speed(const VL& piles, ll h) {
    ll lo = 0, hi = *std::max_element(piles.begin(), piles.end());
    while (hi - lo > 1) {
        const ll mid = lo + (hi - lo) / 2;
        ll hours = 0;
        for (ll p : piles) hours += (p + mid - 1) / mid;
        (hours <= h ? hi : lo) = mid;
    }
    return hi;
}
// P3.5 every value appears three times except one: count each bit modulo 3
std::int32_t single_of_three(const std::vector<std::int32_t>& a) {
    std::uint32_t res = 0;
    for (int b = 0; b < 32; ++b) {
        int c = 0;
        for (std::int32_t x : a) c += int((std::uint32_t(x) >> b) & 1u);
        if (c % 3) res |= 1u << b;
    }
    return std::int32_t(res);                      // C++20: two's complement conversion
}
// P3.6 longest substring with at most k distinct characters
int longest_k_distinct(const std::string& s, int k) {
    int cnt[256] = {}, distinct = 0, best = 0;
    for (int l = 0, r = 0; r < int(s.size()); ++r) {
        if (cnt[(unsigned char)s[r]]++ == 0) ++distinct;
        while (distinct > k)
            if (--cnt[(unsigned char)s[l++]] == 0) --distinct;
        best = std::max(best, r - l + 1);
    }
    return best;
}
// P3.7 split into m contiguous parts minimizing the largest part sum
ll split_largest(const VL& a, ll m) {
    ll lo = *std::max_element(a.begin(), a.end()) - 1, hi = 0;
    for (ll x : a) hi += x;
    while (hi - lo > 1) {
        const ll cap = lo + (hi - lo) / 2;
        ll parts = 1, load = 0;
        for (ll x : a) {
            if (load + x > cap) { ++parts; load = 0; }
            load += x;
        }
        (parts <= m ? hi : lo) = cap;
    }
    return hi;
}
// P3.8 sum of minimums of all subarrays: a[i] is the minimum of left*right subarrays
ll sum_subarray_mins(const VL& a, ll mod) {
    const int n = int(a.size());
    std::vector<int> left(n), right(n), st;
    for (int i = 0; i < n; ++i) {                  // previous strictly smaller element
        while (!st.empty() && a[st.back()] >= a[i]) st.pop_back();
        left[i] = i - (st.empty() ? -1 : st.back());
        st.push_back(i);
    }
    st.clear();
    for (int i = n - 1; i >= 0; --i) {             // next smaller-or-equal element
        while (!st.empty() && a[st.back()] > a[i]) st.pop_back();
        right[i] = (st.empty() ? n : st.back()) - i;
        st.push_back(i);
    }
    ll total = 0;
    for (int i = 0; i < n; ++i)
        total = (total + a[i] % mod * left[i] % mod * right[i]) % mod;
    return total;
}
// P3.9 running median with two heaps; returns 2 * median to stay in integers
VL running_median_x2(const VL& a) {
    std::priority_queue<ll> low;                   // max-heap: smaller half
    std::priority_queue<ll, VL, std::greater<ll>> high;   // min-heap: larger half
    VL out;
    for (ll x : a) {
        if (low.empty() || x <= low.top()) low.push(x); else high.push(x);
        if (low.size() > high.size() + 1) { high.push(low.top()); low.pop(); }
        if (high.size() > low.size()) { low.push(high.top()); high.pop(); }
        out.push_back(low.size() > high.size() ? 2 * low.top() : low.top() + high.top());
    }
    return out;
}
// P3.10 merge ropes: always merge the two shortest
ll rope_cost(const VL& a) {
    std::priority_queue<ll, VL, std::greater<ll>> pq(a.begin(), a.end());
    ll cost = 0;
    while (pq.size() > 1) {
        const ll x = pq.top(); pq.pop();
        const ll y = pq.top(); pq.pop();
        cost += x + y;
        pq.push(x + y);
    }
    return cost;
}
// P3.11 minimum points stabbing all closed intervals: sort by right end
int min_arrows(std::vector<std::pair<ll, ll>> iv) {
    std::sort(iv.begin(), iv.end(), [](auto& p, auto& q) { return p.second < q.second; });
    int arrows = 0;
    ll last = 0;
    for (auto [l, r] : iv)
        if (arrows == 0 || l > last) { ++arrows; last = r; }
    return arrows;
}
// P3.12 shortest subarray with sum >= K (negatives allowed): monotonic deque of prefixes
int shortest_at_least(const VL& a, ll K) {
    const int n = int(a.size());
    VL P(n + 1, 0);
    for (int i = 0; i < n; ++i) P[i + 1] = P[i] + a[i];
    std::deque<int> q;                             // indices with increasing P
    int best = n + 1;
    for (int j = 0; j <= n; ++j) {
        while (!q.empty() && P[j] - P[q.front()] >= K) {
            best = std::min(best, j - q.front());
            q.pop_front();                         // a later j cannot do better with it
        }
        while (!q.empty() && P[q.back()] >= P[j]) q.pop_back();   // j dominates it
        q.push_back(j);
    }
    return best <= n ? best : -1;
}
// P3.13 meet in the middle: subsets with sum exactly T, n up to about 40
ll count_subsets_mitm(const VL& a, ll T) {
    const int n = int(a.size()), h = n / 2;
    auto sums = [&](int from, int to) {
        VL s{0};
        for (int i = from; i < to; ++i) {
            const std::size_t m = s.size();
            for (std::size_t j = 0; j < m; ++j) s.push_back(s[j] + a[i]);
        }
        return s;
    };
    VL L = sums(0, h), R = sums(h, n);
    std::sort(R.begin(), R.end());
    ll cnt = 0;
    for (ll x : L) {
        auto [lo, hi] = std::equal_range(R.begin(), R.end(), T - x);
        cnt += hi - lo;
    }
    return cnt;
}
// P3.14 sum of node heights in a heap with n nodes (array indices 0..n-1)
ll heap_height_sum(int n) {
    std::vector<int> ht(n, 0);
    ll s = 0;
    for (int i = n - 1; i >= 0; --i) {             // children have larger indices
        const int c = 2 * i + 1;
        if (c < n) ht[i] = 1 + std::max(ht[c], c + 1 < n ? ht[c + 1] : 0);
        s += ht[i];
    }
    return s;
}

int main() {
    // ---- P3.2
    VL d = apply_updates(8, {{1, 4, 3}, {3, 8, 2}, {2, 5, -1}, {6, 6, 5}});
    std::printf("P3.2  final array:");
    for (ll x : d) std::printf(" %lld", x);
    std::printf("\n");
    for (int t = 0; t < 500; ++t) {
        const int n = int(rnd(1, 12));
        std::vector<std::array<ll, 3>> ups;
        VL naive(n, 0);
        for (int u = 0; u < 6; ++u) {
            ll l = rnd(1, n), r = rnd(1, n), v = rnd(-9, 9);
            if (l > r) std::swap(l, r);
            ups.push_back({l, r, v});
            for (ll i = l; i <= r; ++i) naive[i - 1] += v;
        }
        check(apply_updates(n, ups) == naive, "P3.2");
    }
    // ---- P3.3
    std::printf("P3.3  count = %lld\n", count_sum_k({1, 2, 3, -3, 1, 2, -1, 3}, 3));
    for (int t = 0; t < 2000; ++t) {
        VL a(rnd(1, 10));
        for (ll& x : a) x = rnd(-3, 3);
        const ll k = rnd(-3, 3);
        ll b = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
            for (ll s = 0, j = ll(i); j < ll(a.size()); ++j) b += (s += a[j]) == k;
        check(count_sum_k(a, k) == b, "P3.3");
    }
    // ---- P3.4
    std::printf("P3.4  speed = %lld (h=5: %lld, h=6: %lld)\n", min_speed({3, 6, 7, 11}, 8),
                min_speed({30, 11, 23, 4, 20}, 5), min_speed({30, 11, 23, 4, 20}, 6));
    for (int t = 0; t < 500; ++t) {
        VL p(rnd(1, 6));
        for (ll& x : p) x = rnd(1, 30);
        const ll h = rnd(ll(p.size()), 40);
        ll s = 1;                                  // brute force: try speeds upward
        for (;; ++s) {
            ll hrs = 0;
            for (ll x : p) hrs += (x + s - 1) / s;
            if (hrs <= h) break;
        }
        check(min_speed(p, h) == s, "P3.4");
    }
    // ---- P3.5
    std::printf("P3.5  %d %d %d\n", single_of_three({2, 2, 3, 2}),
                single_of_three({0, 1, 0, 1, 0, 1, 99}),
                single_of_three({-2, -2, 1, 1, -3, 1, -3, -3, -4, -2}));
    // ---- P3.6
    std::printf("P3.6  eceba,k=2: %d   aaabbccdddd,k=2: %d\n",
                longest_k_distinct("eceba", 2), longest_k_distinct("aaabbccdddd", 2));
    for (int t = 0; t < 2000; ++t) {
        std::string s(rnd(1, 12), 'a');
        for (char& c : s) c = char('a' + rnd(0, 3));
        const int k = int(rnd(1, 3));
        int b = 0;
        for (std::size_t i = 0; i < s.size(); ++i)
            for (std::size_t j = i; j < s.size(); ++j) {
                std::string sub = s.substr(i, j - i + 1);
                std::sort(sub.begin(), sub.end());
                const int dist = int(std::unique(sub.begin(), sub.end()) - sub.begin());
                if (dist <= k) b = std::max(b, int(j - i + 1));
            }
        check(longest_k_distinct(s, k) == b, "P3.6");
    }
    // ---- P3.7
    std::printf("P3.7  %lld %lld\n", split_largest({7, 2, 5, 10, 8}, 2),
                split_largest({1, 2, 3, 4, 5}, 2));
    for (int t = 0; t < 500; ++t) {
        VL a(rnd(1, 8));
        for (ll& x : a) x = rnd(0, 20);
        const ll m = rnd(1, ll(a.size()));
        const int n = int(a.size());               // DP f[j][i]: first i items, j parts
        std::vector<VL> f(m + 1, VL(n + 1, ll(1e18)));
        f[0][0] = 0;
        for (int j = 1; j <= m; ++j)
            for (int i = 1; i <= n; ++i)
                for (ll s = 0, p = i - 1; p >= 0; --p) {
                    s += a[p];
                    f[j][i] = std::min(f[j][i], std::max(f[j - 1][p], s));
                }
        ll want = ll(1e18);
        for (int j = 1; j <= m; ++j) want = std::min(want, f[j][n]);
        check(split_largest(a, m) == want, "P3.7");
    }
    // ---- P3.8
    const ll MOD = 1'000'000'007;
    std::printf("P3.8  %lld %lld\n", sum_subarray_mins({3, 1, 2, 4}, MOD),
                sum_subarray_mins({11, 81, 94, 43, 3}, MOD));
    for (int t = 0; t < 2000; ++t) {
        VL a(rnd(1, 10));
        for (ll& x : a) x = rnd(1, 4);             // many ties
        ll b = 0;
        for (std::size_t i = 0; i < a.size(); ++i)
            for (ll m = a[i], j = ll(i); j < ll(a.size()); ++j) b += m = std::min(m, a[j]);
        check(sum_subarray_mins(a, MOD) == b % MOD, "P3.8");
    }
    // ---- P3.9
    VL med = running_median_x2({5, 15, 1, 3, 8, 7, 9, 10});
    std::printf("P3.9  medians:");
    for (ll x : med) std::printf(" %g", double(x) / 2);
    std::printf("\n");
    for (int t = 0; t < 500; ++t) {
        VL a(rnd(1, 15));
        for (ll& x : a) x = rnd(-5, 5);
        VL got = running_median_x2(a);
        for (std::size_t i = 0; i < a.size(); ++i) {
            VL pre(a.begin(), a.begin() + ll(i) + 1);
            std::sort(pre.begin(), pre.end());
            const std::size_t m = pre.size();
            const ll want = m % 2 ? 2 * pre[m / 2] : pre[m / 2 - 1] + pre[m / 2];
            check(got[i] == want, "P3.9");
        }
    }
    // ---- P3.10
    std::printf("P3.10 %lld %lld\n", rope_cost({4, 3, 2, 6}), rope_cost({1, 8, 3, 5}));
    std::function<ll(VL)> brute_rope = [&](VL a) -> ll {   // try every pair to merge
        if (a.size() < 2) return 0;
        ll best = ll(1e18);
        for (std::size_t i = 0; i < a.size(); ++i)
            for (std::size_t j = i + 1; j < a.size(); ++j) {
                VL b;
                for (std::size_t k = 0; k < a.size(); ++k)
                    if (k != i && k != j) b.push_back(a[k]);
                b.push_back(a[i] + a[j]);
                best = std::min(best, a[i] + a[j] + brute_rope(b));
            }
        return best;
    };
    for (int t = 0; t < 200; ++t) {
        VL a(rnd(1, 5));
        for (ll& x : a) x = rnd(1, 9);
        check(rope_cost(a) == brute_rope(a), "P3.10");
    }
    // ---- P3.11
    std::printf("P3.11 %d\n", min_arrows({{10, 16}, {2, 8}, {1, 6}, {7, 12}}));
    for (int t = 0; t < 500; ++t) {
        std::vector<std::pair<ll, ll>> iv(rnd(1, 7));
        for (auto& [l, r] : iv) { l = rnd(0, 10); r = l + rnd(0, 5); }
        const int n = int(iv.size());
        int best = n;                              // some optimum uses only right ends
        for (unsigned mask = 1; mask < (1u << n); ++mask) {
            bool all = true;
            for (auto [l, r] : iv) {
                bool hit = false;
                for (int b = 0; b < n; ++b)
                    if ((mask >> b) & 1u) hit |= (l <= iv[b].second && iv[b].second <= r);
                all &= hit;
            }
            if (all) best = std::min(best, std::popcount(mask));
        }
        check(min_arrows(iv) == best, "P3.11");
    }
    // ---- P3.12
    std::printf("P3.12 %d %d %d\n", shortest_at_least({2, -1, 2}, 3),
                shortest_at_least({84, -37, 32, 40, 95}, 167),
                shortest_at_least({1, 2}, 4));
    for (int t = 0; t < 2000; ++t) {
        VL a(rnd(1, 10));
        for (ll& x : a) x = rnd(-5, 6);
        const ll K = rnd(1, 12);
        int b = -1;
        for (std::size_t i = 0; i < a.size(); ++i)
            for (ll s = 0, j = ll(i); j < ll(a.size()); ++j)
                if ((s += a[j]) >= K && (b < 0 || j - ll(i) + 1 < b))
                    b = int(j - ll(i) + 1);
        check(shortest_at_least(a, K) == b, "P3.12");
    }
    // ---- P3.13
    VL one_to_40(40);
    for (int i = 0; i < 40; ++i) one_to_40[i] = i + 1;
    std::vector<ll> ways(821, 0);                  // cross-check by subset-sum counting DP
    ways[0] = 1;
    for (ll x : one_to_40)
        for (ll s = 820; s >= x; --s) ways[s] += ways[s - x];
    std::printf("P3.13 subsets of {1..40} with sum 410: %lld (DP: %lld)\n",
                count_subsets_mitm(one_to_40, 410), ways[410]);
    for (int t = 0; t < 300; ++t) {
        VL a(rnd(1, 14));
        for (ll& x : a) x = rnd(-6, 6);
        const ll T = rnd(-10, 10);
        ll b = 0;
        for (unsigned mask = 0; mask < (1u << a.size()); ++mask) {
            ll s = 0;
            for (std::size_t i = 0; i < a.size(); ++i) s += (mask >> i) & 1u ? a[i] : 0;
            b += s == T;
        }
        check(count_subsets_mitm(a, T) == b, "P3.13");
    }
    // ---- P3.14
    int bad = 0;
    for (int n = 1; n <= 20000; ++n)
        bad += heap_height_sum(n) != n - std::popcount(unsigned(n));
    std::printf("P3.14 n - popcount(n) identity: %d failures for n <= 20000\n", bad);
    check(bad == 0, "P3.14");

    std::printf(failures ? "FAILURES: %d\n" : "all random cross-checks passed (%d)\n",
                failures);
}
