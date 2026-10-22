// Checks every numeric answer to the Chapter 2 practice problems, by exact enumeration
// where that is cheap and by simulation (with a 3-standard-error test) otherwise.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <random>
#include <string>
#include <vector>

std::mt19937_64 rng(12345);
double U() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); }
int die() { return std::uniform_int_distribution<int>(1, 6)(rng); }
int coin() { return int(rng() >> 63); }

int failures = 0;
template <class F>
void sim(const char* name, double exact, long long n, F trial) {
    double s = 0, s2 = 0;
    long long used = 0;
    for (long long i = 0; i < n; ++i) {
        double x = trial();
        if (std::isnan(x)) continue;                 // rejected sample (conditioning)
        s += x; s2 += x * x; ++used;
    }
    double m = s / used, se = std::sqrt((s2 / used - m * m) / used);
    bool ok = std::fabs(m - exact) <= 3 * se + 1e-12;
    failures += !ok;
    std::printf("%-40s sim %.4f +- %.4f  exact %.4f  %s\n", name, m, 1.96 * se, exact,
                ok ? "ok" : "FAIL");
}
void exact(const char* name, double got, double want) {
    bool ok = std::fabs(got - want) < 1e-9;
    failures += !ok;
    std::printf("%-40s computed %.6f  answer %.6f  %s\n", name, got, want,
                ok ? "ok" : "FAIL");
}

int main() {
    const long long N = 1'000'000;
    const double NaN = std::nan("");

    std::string w = "MISSISSIPPI";                                       // P2.1
    std::sort(w.begin(), w.end());
    long long cnt = 0;
    do ++cnt; while (std::next_permutation(w.begin(), w.end()));
    exact("P2.1 arrangements of MISSISSIPPI", double(cnt), 34650);

    int with3 = 0, with3sum7 = 0;                                        // P2.2
    for (int a = 1; a <= 6; ++a)
        for (int b = 1; b <= 6; ++b)
            if (a == 3 || b == 3) { ++with3; with3sum7 += (a + b == 7); }
    exact("P2.2 P(sum 7 | at least one 3)", double(with3sum7) / with3, 2.0 / 11);

    auto pick = [&](bool want_next) {                                    // P2.3
        bool two_headed = coin();
        for (int i = 0; i < 3; ++i) if (!two_headed && !coin()) return NaN;
        if (!want_next) return double(two_headed);
        return double(two_headed || coin());
    };
    sim("P2.3a P(two-headed | HHH)", 8.0 / 9, N, [&] { return pick(false); });
    sim("P2.3b P(next flip H | HHH)", 17.0 / 18, N, [&] { return pick(true); });

    exact("P2.4 47*53", 47 * 53, 2491);                                  // P2.4
    std::printf("%-40s 0.97^10 = %.6f, sqrt(120) = %.6f, 1/17 = %.6f\n", "P2.4 (reference)",
                std::pow(0.97, 10), std::sqrt(120.0), 1.0 / 17);

    sim("P2.5 E[distinct faces in 6 rolls]", 6 * (1 - std::pow(5.0 / 6, 6)), N, [] {
        unsigned seen = 0;
        for (int i = 0; i < 6; ++i) seen |= 1u << die();
        return double(__builtin_popcount(seen));
    });

    std::vector<int> p(10);                                              // P2.6
    std::iota(p.begin(), p.end(), 0);
    sim("P2.6a E[fixed points], n=10", 1.0, N, [&] {
        std::shuffle(p.begin(), p.end(), rng);
        int f = 0; for (int i = 0; i < 10; ++i) f += (p[i] == i);
        return double(f);
    });
    sim("P2.6b E[(fixed points)^2] = Var+1, n=10", 2.0, N, [&] {
        std::shuffle(p.begin(), p.end(), rng);
        int f = 0; for (int i = 0; i < 10; ++i) f += (p[i] == i);
        return double(f * f);
    });
    std::array<int, 4> q{0, 1, 2, 3};
    int der = 0, tot = 0;
    do { ++tot; bool d = true; for (int i = 0; i < 4; ++i) d &= (q[i] != i); der += d; }
    while (std::next_permutation(q.begin(), q.end()));
    exact("P2.6c P(derangement), n=4", double(der) / tot, 3.0 / 8);

    std::vector<int> deck(52);                                           // P2.7
    for (int i = 0; i < 52; ++i) deck[i] = (i < 4);                      // 1 = ace
    sim("P2.7 E[cards until first ace]", 53.0 / 5, N, [&] {
        std::shuffle(deck.begin(), deck.end(), rng);
        int k = 0; while (!deck[k]) ++k;
        return double(k + 1);
    });

    auto wait = [&](const std::string& pat) {                            // P2.8
        std::string h;
        for (int t = 1;; ++t) {
            h += coin() ? 'H' : 'T';
            if (h.ends_with(pat)) return double(t);
        }
    };
    sim("P2.8a E[tosses until HTH]", 10, N / 4, [&] { return wait("HTH"); });
    sim("P2.8b E[tosses until HHT]", 8, N / 4, [&] { return wait("HHT"); });

    sim("P2.9 E[rolls until 66]", 42, N, [] {                            // P2.9
        int prev = 0;
        for (int t = 1;; ++t) {
            int d = die();
            if (d == 6 && prev == 6) return double(t);
            prev = d;
        }
    });

    auto ruin = [](bool want_time) {                                     // P2.10
        int x = 3, t = 0;
        while (x > 0 && x < 10) { x += coin() ? 1 : -1; ++t; }
        return want_time ? double(t) : double(x == 10);
    };
    sim("P2.10a P(reach 10 from 3)", 0.3, N, [&] { return ruin(false); });
    sim("P2.10b E[duration]", 21, N, [&] { return ruin(true); });

    for (int thr : {3, 4}) {                                             // P2.11
        sim(thr == 3 ? "P2.11 pay-$1 reroll, keep >= 3" : "P2.11 pay-$1 reroll, keep >= 4",
            4.0, N, [thr] {
                double paid = 0;
                for (;;) { int d = die(); if (d >= thr) return d - paid; paid += 1; }
            });
    }

    sim("P2.12 P(biased walk from 3 hits 0)", 8.0 / 27, N, [] {          // P2.12
        int x = 3;
        while (x > 0 && x < 60) x += (U() < 0.6) ? 1 : -1;   // return from 60: ~(2/3)^60
        return double(x == 0);
    });

    sim("P2.13 P(triangle), break longer piece", 2 * std::log(2.0) - 1, N, [] {  // P2.13
        double x = U(), L = std::fmax(x, 1 - x), s = 1 - L, a = U() * L, b = L - a;
        return double(s < 0.5 && a < 0.5 && b < 0.5);
    });

    sim("P2.14 E[rolls until 6 | all even]", 1.5, 4 * N, [&] {           // P2.14
        for (int t = 1;; ++t) {
            int d = die();
            if (d == 6) return double(t);
            if (d % 2 == 1) return NaN;                                  // condition fails
        }
    });

                                                                         // P2.15
    sim("P2.15 P(last passenger gets own seat)", 0.5, N / 5, [&] {
        std::vector<char> taken(100, 0);
        int s0 = std::uniform_int_distribution<int>(0, 99)(rng);         // passenger 0 lost
        taken[s0] = 1;
        for (int i = 1; i < 99; ++i) {
            if (!taken[i]) { taken[i] = 1; continue; }
            int free = 0;                                    // pick a random free seat
            for (int k = 0; k < 100; ++k) free += !taken[k];
            int r = std::uniform_int_distribution<int>(0, free - 1)(rng);
            for (int k = 0; k < 100; ++k)
                if (!taken[k] && r-- == 0) { taken[k] = 1; break; }
        }
        return double(!taken[99]);
    });

    std::printf("\n%s\n", failures ? "SOME CHECKS FAILED" : "All checks passed.");
    return failures ? 1 : 0;
}
