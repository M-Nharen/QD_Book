// Lab kernel for Section 16.1: the same two reductions compiled under different flags.
// Prints nanoseconds per element for an int sum and a float sum over 4,096 elements.
#include <chrono>
#include <cstdio>
#include <vector>

[[gnu::noinline]] int sum_int(const int* a, int n) {
    int s = 0;
    for (int i = 0; i < n; ++i) s += a[i];
    return s;
}

[[gnu::noinline]] float sum_float(const float* a, int n) {
    float s = 0;
    for (int i = 0; i < n; ++i) s += a[i];            // (a + b) + c != a + (b + c)
    return s;
}

template <class F>
double ns_per_element(F f, int n) {
    constexpr int kReps = 20000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < kReps; ++r) f();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / kReps / n;
}

int main() {
    constexpr int n = 4096;
    std::vector<int> a(n, 1);
    std::vector<float> b(n, 1.0f);
    volatile int si = 0;                              // volatile sinks keep the results
    volatile float sf = 0;
    const double ti = ns_per_element([&] { si = si + sum_int(a.data(), n); }, n);
    const double tf = ns_per_element([&] { sf = sf + sum_float(b.data(), n); }, n);
    std::printf("%.3f %.3f\n", ti, tf);
}
