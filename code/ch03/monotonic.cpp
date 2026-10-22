// Monotonic stack: next greater element and largest rectangle in a histogram (3.6).
#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>
using ll = long long;

// nge[i] = index of the first j > i with a[j] > a[i], or n if none. O(n) amortized.
std::vector<int> next_greater(const std::vector<int>& a) {
    const int n = int(a.size());
    std::vector<int> nge(n, n), st;                // st: indices, values non-increasing
    st.reserve(n);
    for (int i = 0; i < n; ++i) {
        while (!st.empty() && a[st.back()] < a[i]) {   // a[i] is the answer for these
            nge[st.back()] = i;
            st.pop_back();
        }
        st.push_back(i);
    }
    return nge;
}

// Largest rectangle: for each bar, extend left and right while bars are >= it. O(n).
ll largest_rectangle(const std::vector<int>& h) {
    const int n = int(h.size());
    std::vector<int> st;                           // indices, heights strictly increasing
    ll best = 0;
    for (int i = 0; i <= n; ++i) {
        const int cur = (i == n) ? 0 : h[i];       // sentinel bar of height 0 flushes all
        while (!st.empty() && h[st.back()] >= cur) {
            const int top = st.back();
            st.pop_back();
            const int left = st.empty() ? -1 : st.back();   // first bar < h[top] on left
            best = std::max(best, ll(h[top]) * (i - left - 1));
        }
        st.push_back(i);
    }
    return best;
}

ll largest_rectangle_brute(const std::vector<int>& h) {    // O(n^2) reference
    ll best = 0;
    for (std::size_t i = 0; i < h.size(); ++i) {
        int lo = h[i];
        for (std::size_t j = i; j < h.size(); ++j) {
            lo = std::min(lo, h[j]);
            best = std::max(best, ll(lo) * ll(j - i + 1));
        }
    }
    return best;
}

int main() {
    const std::vector<int> a{2, 7, 3, 5, 4, 6, 8};
    std::printf("next greater index:");
    for (int j : next_greater(a)) std::printf(" %d", j);
    std::printf("\nhistogram [2,1,5,6,2,3]: %lld\n", largest_rectangle({2, 1, 5, 6, 2, 3}));

    std::mt19937 rng(7);
    std::uniform_int_distribution<int> len(1, 12), val(0, 6);   // many ties on purpose
    int bad = 0;
    for (int t = 0; t < 20000; ++t) {
        std::vector<int> h(len(rng));
        for (int& x : h) x = val(rng);
        bad += largest_rectangle(h) != largest_rectangle_brute(h);
    }
    std::printf("random cross-check vs O(n^2): %d mismatches in 20000\n", bad);
}
