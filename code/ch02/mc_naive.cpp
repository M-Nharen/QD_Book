// Naive Monte Carlo estimator. It "works", but has four problems (see Section 2.11).
#include <cstdio>
#include <cstdlib>
#include <ctime>

int main() {
    std::srand(std::time(nullptr));              // (1) seed differs every run
    const int N = 1'000'000;

    int hits = 0;
    for (int i = 0; i < N; ++i) {
        int a = std::rand() % 6 + 1;             // (2) modulo bias, (3) hidden global state
        int b = std::rand() % 6 + 1;
        if (a + b == 7) ++hits;
    }
    std::printf("P(sum of two dice = 7) ~ %f\n", double(hits) / N);   // (4) no error bar

    long long total = 0;                         // expected flips until two heads in a row
    for (int t = 0; t < N; ++t) {
        int flips = 0, prev = 0;
        for (;;) {
            int c = std::rand() % 2;
            ++flips;
            if (c == 1 && prev == 1) break;
            prev = c;
        }
        total += flips;
    }
    std::printf("E[flips until HH]      ~ %f\n", double(total) / N);
}
