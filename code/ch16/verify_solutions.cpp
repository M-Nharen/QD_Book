// Checks the numeric answers to the Chapter 16 practice problems. P16.5 is answered by
// simulating both load generators against the same single-server queue.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Result { std::size_t n; double p50, p99, max, over_1ms; };

Result summarize(std::vector<double> v) {             // latencies in microseconds
    std::sort(v.begin(), v.end());
    auto rank = [&](double p) {                       // nearest rank
        return v[std::size_t(std::ceil(p * double(v.size()))) - 1];
    };
    const auto slow = std::count_if(v.begin(), v.end(), [](double x) { return x > 1000; });
    return {v.size(), rank(0.50), rank(0.99), v.back(), double(slow) / double(v.size())};
}

// Server: 20 us per request, except that it makes no progress during [500000, 510000) us.
double finish_time(double start) {
    double end = start + 20;
    if (start < 510'000 && end > 500'000) end += 10'000;
    return end;
}

int main() {
    // ---- P16.1 div10 by multiply-shift, checked for every 32-bit input
    std::uint64_t bad = 0;
    for (std::uint64_t x = 0; x <= 0xffffffffu; ++x)
        bad += ((x * 0xcccccccdull) >> 35) != x / 10;
    std::printf("P16.1 (x * 0xcccccccd) >> 35 == x / 10 for all 2^32 x: %s\n",
                bad ? "NO" : "yes");

    // ---- P16.2 TSC arithmetic
    std::printf("P16.2 per call %.2f ns\n", (1550.0 - 92.0) / 32 / 2.1);

    // ---- P16.4 histogram percentiles and bounds on the mean
    const double lo[] = {1, 2, 3, 5, 10, 50, 100, 1000}, hi[] = {2, 3, 5, 10, 50, 100, 1000,
                                                                 10000};
    const double cnt[] = {520000, 300000, 150000, 20000, 8000, 1500, 400, 100};
    double total = 0, mlo = 0, mhi = 0;
    for (int i = 0; i < 8; ++i) {
        total += cnt[i];
        mlo += cnt[i] * lo[i];
        mhi += cnt[i] * hi[i];
    }
    for (double p : {0.5, 0.99, 0.999, 0.9999}) {
        double cum = 0;
        int b = 0;
        while (cum + cnt[b] < std::ceil(p * total)) cum += cnt[b++];
        std::printf("P16.4 p%g in [%g, %g) us\n", p * 100, lo[b], hi[b]);
    }
    std::printf("      mean between %.3f and %.3f us; slowest bucket adds %.3f-%.3f us\n",
                mlo / total, mhi / total, 100 * 1000 / total, 100 * 10000 / total);

    // ---- P16.5 closed loop (one request outstanding) vs open loop (10,000 per second)
    std::vector<double> closed, open;
    for (double t = 0; t < 1'000'000;) {
        const double e = finish_time(t);
        closed.push_back(e - t);
        t = e;                                        // next request only after the reply
    }
    double free_at = 0;
    for (int k = 0; k < 10'000; ++k) {
        const double arrive = k * 100.0, start = std::max(arrive, free_at);
        free_at = finish_time(start);
        open.push_back(free_at - arrive);
    }
    for (auto [name, v] : {std::pair{"closed", &closed}, std::pair{"open", &open}}) {
        const Result r = summarize(*v);
        std::printf("P16.5 %-6s n=%5zu p50 %4.0f us p99 %6.0f us max %6.0f us"
                    " >1ms %.3f%%\n", name, r.n, r.p50, r.p99, r.max, 100 * r.over_1ms);
    }

    // ---- P16.6 sampling error
    const double n = 10'000 * 30.0, k = 600, p = k / n, se = std::sqrt(p * (1 - p) / n);
    std::printf("P16.6 share %.4f%% +/- %.4f%% (95%%); CPU time %.3f s of 30 s\n",
                100 * p, 196 * se, p * 30);

    // ---- P16.7 counter arithmetic (hypothetical counts given in the problem)
    const double cyc = 4.2e9, ins = 2.1e9, br = 400e6, brm = 42e6, llc = 30e6;
    std::printf("P16.7 IPC %.2f; branch misses %.1f%%; branch MPKI %.1f; LLC MPKI %.1f\n",
                ins / cyc, 100 * brm / br, brm / (ins / 1000), llc / (ins / 1000));
    std::printf("      mispredicts at 15 cycles = %.0f%% of cycles;\n"
                "      LLC misses at 200 cycles = %.0f%% if none overlapped\n",
                100 * brm * 15 / cyc, 100 * llc * 200 / cyc);
}
