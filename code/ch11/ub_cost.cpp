// What the UB-safe idioms cost at run time (Section 11.8). Single-threaded, -O2.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

template <class F>
double ns_per_elem(F&& f, std::size_t elems, int reps, long long& sink) {
    double best = 1e300;                                    // best of 15 trials
    for (int trial = 0; trial < 15; ++trial) {
        auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < reps; ++r) sink += f();
        auto t1 = std::chrono::steady_clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        if (ns < best) best = ns;
    }
    return best / (double(reps) * elems);
}

[[gnu::noipa]] long long seq_brackets(const std::vector<int>& v) {
    long long s = 0;
    for (std::size_t i = 0; i < v.size(); ++i) s += v[i];
    return s;
}
[[gnu::noipa]] long long seq_at(const std::vector<int>& v) {
    long long s = 0;
    for (std::size_t i = 0; i < v.size(); ++i) s += v.at(i);   // check provably redundant
    return s;
}
[[gnu::noipa]] long long gather_brackets(const std::vector<int>& v,
                                         const std::vector<std::uint32_t>& idx) {
    long long s = 0;
    for (std::uint32_t i : idx) s += v[i];
    return s;
}
[[gnu::noipa]] long long gather_at(const std::vector<int>& v,
                                   const std::vector<std::uint32_t>& idx) {
    long long s = 0;
    for (std::uint32_t i : idx) s += v.at(i);                 // compare + branch per access
    return s;
}
[[gnu::noipa]] long long sum_checked32(const std::vector<int>& v) {
    int s = 0;                                                // 32-bit, overflow detected
    for (int x : v)
        if (__builtin_add_overflow(s, x, &s)) return -1;
    return s;
}

int main() {
    constexpr std::size_t N = 1 << 14;                         // 64 KiB of int: L2-resident
    std::vector<int> v(N);
    std::vector<std::uint32_t> idx(N);
    std::uint64_t s = 42;
    for (std::size_t i = 0; i < N; ++i) {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        v[i] = int(s >> 54);                                  // 0..1023: no overflow
        idx[i] = std::uint32_t((s >> 20) % N);
    }
    long long sink = 0;
    const int R = 300;
    std::printf("sequential v[i]      %.3f ns/elem\n",
                ns_per_elem([&] { return seq_brackets(v); }, N, R, sink));
    std::printf("sequential v.at(i)   %.3f ns/elem\n",
                ns_per_elem([&] { return seq_at(v); }, N, R, sink));
    std::printf("random     v[idx]    %.3f ns/elem\n",
                ns_per_elem([&] { return gather_brackets(v, idx); }, N, R, sink));
    std::printf("random     v.at(idx) %.3f ns/elem\n",
                ns_per_elem([&] { return gather_at(v, idx); }, N, R, sink));
    std::printf("checked int32 sum    %.3f ns/elem\n",
                ns_per_elem([&] { return sum_checked32(v); }, N, R, sink));
    std::printf("(checksum %lld)\n", sink);
}
