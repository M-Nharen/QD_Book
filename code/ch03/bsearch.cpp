// Binary search from one invariant: lo is always "false", hi is always "true" (3.5).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>
using ll = long long;

// Precondition: pred is monotone on (lo, hi]: false...false true...true, and pred(hi) is
// true or hi is a virtual "past the end" value. lo is never evaluated (it may be virtual).
// Returns the smallest x in (lo, hi] with pred(x) true (hi if none in (lo, hi)).
template <class Pred>
ll first_true(ll lo, ll hi, Pred pred) {
    while (hi - lo > 1) {                          // invariant: !pred(lo) and pred(hi)
        const ll mid = lo + (hi - lo) / 2;         // no overflow, and lo < mid < hi
        if (pred(mid)) hi = mid; else lo = mid;
    }
    return hi;
}

// lower_bound: first index i with a[i] >= x, or n.
ll my_lower_bound(const std::vector<int>& a, int x) {
    return first_true(-1, ll(a.size()), [&](ll i) { return a[i] >= x; });
}

// floor(sqrt(n)) for 0 <= n <= 10^18 without floating point.
ll isqrt(ll n) {                                   // (first x with x*x > n) - 1
    return first_true(0, 2'000'000'000LL, [&](ll x) { return x > n / x; }) - 1;
}

// Binary search on the answer: least ship capacity that delivers in order within `days`.
ll ship_capacity(const std::vector<ll>& w, ll days) {
    const ll lo = *std::max_element(w.begin(), w.end()) - 1;   // infeasible
    const ll hi = std::accumulate(w.begin(), w.end(), 0LL);    // always feasible
    auto feasible = [&](ll cap) {                  // greedy: fill each day maximally
        ll used = 1, load = 0;
        for (ll x : w) {
            if (load + x > cap) { ++used; load = 0; }
            load += x;
        }
        return used <= days;
    };
    return first_true(lo, hi, feasible);
}

int main() {
    const std::vector<int> a{1, 3, 3, 3, 7, 9};
    for (int x : {0, 3, 4, 9, 10}) {
        const ll mine = my_lower_bound(a, x);
        const ll stl = std::lower_bound(a.begin(), a.end(), x) - a.begin();
        std::printf("lower_bound(%2d) = %lld (std: %lld)\n", x, mine, stl);
    }
    const ll n = 999'999'999'999'999'999LL;        // 10^18 - 1
    std::printf("isqrt(10^18 - 1) = %lld, via double: %lld\n",
                isqrt(n), ll(std::sqrt(double(n))));
    std::printf("isqrt(10^18)     = %lld\n", isqrt(n + 1));

    std::vector<ll> w(10);
    std::iota(w.begin(), w.end(), 1);              // weights 1..10
    std::printf("ship 1..10 in 5 days: capacity %lld\n", ship_capacity(w, 5));

    double lo = 1.0, hi = 2.0;                     // real-valued: cube root of 2
    for (int it = 0; it < 100; ++it) {             // fixed count: each halves the interval
        const double mid = (lo + hi) / 2;
        (mid * mid * mid >= 2.0 ? hi : lo) = mid;
    }
    std::printf("cbrt(2) ~ %.15f (std::cbrt %.15f)\n", hi, std::cbrt(2.0));
}
