// Rolling maximum in O(n): monotonic deque, then the same idea in a fixed ring (3.10).
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <random>
#include <set>
#include <vector>

std::vector<int> make_prices(std::size_t n) {      // identical to Listing 3.8
    static const int step[4] = {-2, -1, 1, 2};
    std::mt19937 rng(2024);
    std::vector<int> p(n);
    int px = 100'000;
    for (int& v : p) v = px += step[rng() >> 30];
    return p;
}

// (a) Balanced tree: O(n log k), one node allocation and one free per price.
void max_multiset(const std::vector<int>& p, std::size_t k, std::vector<int>& out) {
    std::multiset<int> w;
    for (std::size_t i = 0; i < p.size(); ++i) {
        w.insert(p[i]);
        if (i >= k) w.erase(w.find(p[i - k]));     // erase ONE copy, not every equal value
        if (i + 1 >= k) out[i + 1 - k] = *w.rbegin();
    }
}

// (b) Monotonic deque of indices; values strictly decrease from front to back. O(n).
void max_deque(const std::vector<int>& p, std::size_t k, std::vector<int>& out) {
    std::deque<std::size_t> q;
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (!q.empty() && q.front() + k <= i) q.pop_front();      // left the window
        while (!q.empty() && p[q.back()] <= p[i]) q.pop_back();   // can never be the max
        q.push_back(i);
        if (i + 1 >= k) out[i + 1 - k] = p[q.front()];
    }
}

// (c) Same algorithm in a power-of-two ring buffer: no allocation inside the loop.
void max_ring(const int* p, std::size_t n, std::size_t k, int* out) {
    const std::size_t cap = std::bit_ceil(k), mask = cap - 1;
    std::vector<std::uint32_t> q(cap);             // the only allocation, before the loop
    std::size_t head = 0, tail = 0;                // live entries: q[head..tail) mod cap
    for (std::size_t i = 0; i < n; ++i) {
        if (head != tail && q[head & mask] + k <= i) ++head;
        while (head != tail && p[q[(tail - 1) & mask]] <= p[i]) --tail;
        q[tail++ & mask] = std::uint32_t(i);       // size <= k <= cap after this push
        if (i + 1 >= k) out[i + 1 - k] = p[q[head & mask]];
    }
}

bool small_cross_check() {                         // all three against brute force
    std::mt19937 rng(1);
    for (int t = 0; t < 3000; ++t) {
        const std::size_t n = 1 + rng() % 40, k = 1 + rng() % n;
        std::vector<int> p(n);
        for (int& v : p) v = int(rng() % 7);       // small range: many equal values
        std::vector<int> a(n - k + 1), b(a), c(a);
        max_multiset(p, k, a);
        max_deque(p, k, b);
        max_ring(p.data(), n, k, c.data());
        for (std::size_t i = 0; i + k <= n; ++i) {
            int m = p[i];
            for (std::size_t j = i; j < i + k; ++j) m = std::max(m, p[j]);
            if (a[i] != m || b[i] != m || c[i] != m) return false;
        }
    }
    return true;
}

template <class F>
void timed(const char* name, std::size_t k, std::size_t n, F f, std::vector<int>& out) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    long long sum = 0;
    for (int v : out) sum += v;
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    std::printf("k=%5zu %-9s checksum %lld  %6.2f ns per price\n", k, name, sum,
                ns / double(n));
}

int main() {
    std::printf("cross-check on small random inputs: %s\n",
                small_cross_check() ? "ok" : "BUG");
    const std::size_t n = 2'000'000;
    const std::vector<int> p = make_prices(n);
    for (std::size_t k : {8, 64, 1024}) {
        std::vector<int> out(n - k + 1);
        timed("multiset", k, n, [&] { max_multiset(p, k, out); }, out);
        timed("deque", k, n, [&] { max_deque(p, k, out); }, out);
        timed("ring", k, n, [&] { max_ring(p.data(), n, k, out.data()); }, out);
    }
    std::vector<int> rising(n), out(n - 1024 + 1);  // same code, predictable branches
    for (std::size_t i = 0; i < n; ++i) rising[i] = int(i);
    timed("ring/rise", 1024, n,
          [&] { max_ring(rising.data(), n, 1024, out.data()); }, out);
}
