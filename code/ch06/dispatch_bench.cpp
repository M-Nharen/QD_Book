// book: timeout=240
// What a virtual call costs: predictable vs unpredictable targets, vs a direct call (6.5).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

struct Instrument {
    virtual ~Instrument() = default;
    virtual long value(long mid) const = 0;
};
struct Equity final : Instrument {
    long value(long mid) const override { return mid; }
};
struct Future final : Instrument {
    long basis = 3;
    long value(long mid) const override { return mid + basis; }
};
struct Option final : Instrument {
    long delta = 512;                            // in 1/1024 units
    long value(long mid) const override { return (mid * delta) >> 10; }
};

// The loops live in non-inlined functions so each is compiled exactly as written.
__attribute__((noinline)) long run_virtual(const Instrument* const* obj,
                                           const std::uint8_t* kind, long n) {
    long sum = 0;
    for (long i = 0; i < n; ++i) sum += obj[kind[i]]->value(i);
    return sum;
}
__attribute__((noinline)) long run_direct(const Equity& e, long n) {
    long sum = 0;
    for (long i = 0; i < n; ++i) sum += e.value(i);   // final: no dispatch, inlined
    return sum;
}

template <class F>
double ns_per_call(F f, long n, long& check) {
    double best = 1e30;
    for (int rep = 0; rep < 5; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        check += f();
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
        best = ns < best ? ns : best;
    }
    return best;
}

int main() {
    const long n = 20'000'000;
    Equity eq;
    Future fu;
    Option op;
    const Instrument* obj[3] = {&eq, &fu, &op};
    std::vector<std::uint8_t> same(n, 0), cycle(n), random(n);
    std::uint64_t x = 88172645463325252ULL;
    for (long i = 0; i < n; ++i) {
        cycle[i] = static_cast<std::uint8_t>(i % 3);
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        random[i] = static_cast<std::uint8_t>((x >> 32) % 3);
    }
    long check = 0;
    std::printf("ns per call (best of 5 runs of %ld calls):\n", n);
    std::printf("  direct call to a final class (inlined) %6.2f\n",
                ns_per_call([&] { return run_direct(eq, n); }, n, check));
    std::printf("  virtual, always the same target        %6.2f\n",
                ns_per_call([&] { return run_virtual(obj, same.data(), n); }, n, check));
    std::printf("  virtual, targets cycle A,B,C,A,B,C     %6.2f\n",
                ns_per_call([&] { return run_virtual(obj, cycle.data(), n); }, n, check));
    std::printf("  virtual, targets in random order       %6.2f\n",
                ns_per_call([&] { return run_virtual(obj, random.data(), n); }, n, check));
    std::printf("(checksum %ld)\n", check);
}
