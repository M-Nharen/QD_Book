// Reproducible, fast Monte Carlo with confidence intervals (Section 2.11).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

// SplitMix64: used only to expand one 64-bit seed into generator state.
struct SplitMix64 {
    std::uint64_t x;
    std::uint64_t next() {
        std::uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
};

// xoshiro256** (Blackman & Vigna): small state, very fast, good statistical quality.
// Satisfies UniformRandomBitGenerator, so it also works with <random> distributions.
class Xoshiro256ss {
    std::uint64_t s[4];
    static std::uint64_t rotl(std::uint64_t v, int k) { return (v << k) | (v >> (64 - k)); }
public:
    using result_type = std::uint64_t;
    explicit Xoshiro256ss(std::uint64_t seed) {
        SplitMix64 sm{seed};
        for (auto& w : s) w = sm.next();
    }
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
    result_type operator()() {
        const std::uint64_t result = rotl(s[1] * 5, 7) * 9;
        const std::uint64_t t = s[1] << 17;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t;    s[3] = rotl(s[3], 45);
        return result;
    }
    // Integer in [0, n) by multiply-shift (Lemire). Bias is at most n / 2^64: negligible.
    std::uint64_t below(std::uint64_t n) {
        unsigned __int128 wide = static_cast<unsigned __int128>((*this)()) * n;
        return static_cast<std::uint64_t>(wide >> 64);
    }
    double uniform01() { return ((*this)() >> 11) * 0x1.0p-53; }   // 53 random bits
};

// Welford's online mean/variance: numerically stable, one pass, O(1) memory.
struct Estimate {
    long long n = 0;
    double mean = 0, m2 = 0;
    void add(double x) {
        ++n;
        double d = x - mean;
        mean += d / n;
        m2 += d * (x - mean);
    }
    double stderr_() const { return std::sqrt(m2 / (n - 1) / n); }
};

template <class Trial>
Estimate run(Trial trial, long long n, std::uint64_t seed) {
    Xoshiro256ss rng(seed);
    Estimate e;
    for (long long i = 0; i < n; ++i) e.add(trial(rng));
    return e;
}

void report(const char* name, const Estimate& e, double exact) {
    double se = e.stderr_();
    std::printf("%-30s %9.5f +- %7.5f  exact %9.5f  %s\n", name, e.mean, 1.96 * se, exact,
                std::fabs(e.mean - exact) <= 3 * se ? "ok" : "CHECK");
}

int main() {
    const long long N = 2'000'000;
    const std::uint64_t SEED = 2026;                       // fixed seed: reproducible

    report("P(two dice sum to 7)", run([](Xoshiro256ss& r) {
        return double(r.below(6) + r.below(6) + 2 == 7);
    }, N, SEED), 1.0 / 6);

    auto wait_for = [](int first, int second) {     // flips until (first, second) appears
        return [=](Xoshiro256ss& r) {
            int prev = -1, flips = 0;
            for (;;) {
                int c = int(r() >> 63);                    // top bit = fair coin
                ++flips;
                if (prev == first && c == second) return double(flips);
                prev = c;
            }
        };
    };
    report("E[flips until HH]", run(wait_for(1, 1), N, SEED), 6.0);
    report("E[flips until HT]", run(wait_for(1, 0), N, SEED), 4.0);

    report("P(broken stick -> triangle)", run([](Xoshiro256ss& r) {
        double u = r.uniform01(), v = r.uniform01();
        double a = std::fmin(u, v), b = std::fmax(u, v);
        double x = a, y = b - a, z = 1 - b;                // the three pieces
        return double(x < 0.5 && y < 0.5 && z < 0.5);
    }, N, SEED), 0.25);

    report("E[rolls to see all 6 faces]", run([](Xoshiro256ss& r) {
        unsigned seen = 0; int rolls = 0;
        while (seen != 0x3F) { seen |= 1u << r.below(6); ++rolls; }
        return double(rolls);
    }, N, SEED), 14.7);

    report("Dice game, one re-roll", run([](Xoshiro256ss& r) {
        int first = int(r.below(6)) + 1;
        return double(first >= 4 ? first : int(r.below(6)) + 1);
    }, N, SEED), 4.25);
}
