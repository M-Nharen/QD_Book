// Lab kernel for Section 16.1 (profile-guided optimization): virtual calls through a
// base pointer where one implementation receives 96% of the calls. argv[1] scales the
// run, so the training run can be shorter than the measured one.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

struct Handler {
    virtual ~Handler() = default;
    virtual std::int64_t on(std::int64_t x) = 0;
};
struct Add final : Handler { std::int64_t on(std::int64_t x) override { return x + 3; } };
struct Mul final : Handler { std::int64_t on(std::int64_t x) override { return x * 3; } };
struct Neg final : Handler { std::int64_t on(std::int64_t x) override { return -x; } };

[[gnu::noinline]] Handler* make(unsigned r) {         // hides the types from the optimizer
    if (r < 96) return new Add;
    return r < 98 ? static_cast<Handler*>(new Mul) : new Neg;
}

int main(int argc, char** argv) {
    const int reps = argc > 1 ? std::atoi(argv[1]) : 5000;
    std::mt19937 rng(1);
    std::vector<Handler*> hs;
    for (int i = 0; i < 4096; ++i) hs.push_back(make(rng() % 100));
    std::int64_t s = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < reps; ++r)
        for (Handler* h : hs) s = h->on(s) & 0xffff;
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("%.3f\n", std::chrono::duration<double, std::nano>(t1 - t0).count() /
                              (double(reps) * 4096) + 0 * double(s));
    for (Handler* h : hs) delete h;
}
