// How Monte Carlo error shrinks with the number of trials (Section 2.12).
// Estimates P(broken stick forms a triangle) = 1/4 with growing N.
#include <cmath>
#include <cstdio>
#include <random>

int main() {
    std::mt19937_64 rng(7);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    const double exact = 0.25;
    std::printf("%10s %10s %10s %14s\n", "N", "estimate", "|error|", "predicted SE");
    long long hits = 0, done = 0;
    for (long long N = 100; N <= 100'000'000; N *= 10) {
        for (; done < N; ++done) {                    // reuse earlier trials
            double u = U(rng), v = U(rng);
            double a = std::fmin(u, v), b = std::fmax(u, v);
            hits += (a < 0.5 && b - a < 0.5 && 1 - b < 0.5);
        }
        double est = double(hits) / N;
        double se = std::sqrt(exact * (1 - exact) / N); // sigma / sqrt(N) for a 0/1 trial
        std::printf("%10lld %10.6f %10.6f %14.6f\n", N, est, std::fabs(est - exact), se);
    }
}
