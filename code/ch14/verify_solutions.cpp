// Checks the numeric answers to the Chapter 14 practice problems (Appendix A).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>

int main() {
    // ---- P14.3 index wrap-around
    const double year = 365.25 * 86400;
    std::printf("P14.3 (a) 64-bit index at 1e9/s wraps after %.1f years\n",
                std::ldexp(1.0, 64) / 1e9 / year);
    std::printf("      (b) 32-bit index at 1e8/s wraps after %.2f s\n",
                std::ldexp(1.0, 32) / 1e8);
    std::printf("      (c) 2^32 mod 1000 = %llu, 2^32 mod 1024 = %llu\n",
                (unsigned long long)((1ull << 32) % 1000),
                (unsigned long long)((1ull << 32) % 1024));
    {   // with 32-bit counters and capacity 1000, the slot index jumps at the wrap:
        std::uint32_t c = 0xffff'ffffu;
        const unsigned before = c % 1000;
        ++c;
        std::printf("      slot before wrap %u, after wrap %u (should be %u)\n",
                    before, c % 1000, (before + 1) % 1000);
    }

    // ---- P14.7 burst absorption: 50,000 arrivals 100 ns apart, service 250 ns each
    {
        const long n = 50'000, gap = 100, service = 250;
        long queued = 0, peak = 0, next_arrival = 0, arrived = 0, busy_until = 0;
        long done = 0, t = 0;
        while (done < n) {
            // advance to the next event: an arrival or a service completion
            const long t_arr = arrived < n ? next_arrival : -1;
            const long t_srv = queued > 0 ? busy_until : -1;
            if (t_arr >= 0 && (t_srv < 0 || t_arr <= t_srv)) {
                t = t_arr;
                if (queued == 0) busy_until = t + service;   // idle server starts at once
                ++queued;
                ++arrived;
                next_arrival += gap;
            } else {
                t = t_srv;
                --queued;
                ++done;
                if (queued > 0) busy_until = t + service;
            }
            if (queued > peak) peak = queued;
        }
        std::printf("P14.7 peak occupancy %ld (including the one in service), "
                    "last done at %.3f ms\n", peak, double(t) / 1e6);
    }

    // ---- P14.8 seqlock retry probability: writes every 1000 ns, write window 20 ns,
    //      read window 30 ns, read start uniform. Monte Carlo with a fixed seed.
    {
        std::mt19937_64 rng(14);
        std::uniform_real_distribution<double> u(0.0, 1e6);
        const long trials = 10'000'000;
        long retry = 0;
        for (long i = 0; i < trials; ++i) {
            const double t = u(rng);
            const double k = std::floor((t + 30.0) / 1000.0);   // nearest write at k*1000
            const double w = k * 1000.0;                         // window [w, w+20)
            retry += (t < w + 20.0 && t + 30.0 > w);
        }
        const double p = double(retry) / trials;
        std::printf("P14.8 P(first attempt retries) = %.4f (exact 0.05), "
                    "E[attempts] = %.4f (exact %.4f)\n", p, 1 / (1 - p), 1 / 0.95);
    }

    // ---- P14.9 one contended line, 60 ns per ownership transfer
    std::printf("P14.9 max successful CAS on one line: %.1f million per second\n",
                1e9 / 60.0 / 1e6);

    // ---- P14.10 tag wrap at 25 million pops per second
    for (int bits : {16, 32, 44})
        std::printf("P14.10 %2d-bit tag wraps every %.6g s\n", bits,
                    std::ldexp(1.0, bits) / 25e6);
    std::printf("       2^44 / 25e6 s = %.2f days\n", std::ldexp(1.0, 44) / 25e6 / 86400);
}
