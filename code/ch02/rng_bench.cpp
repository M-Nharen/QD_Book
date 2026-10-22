// Cost per random number for common generators and range-reduction methods (Section 2.12).
// Numbers depend heavily on CPU, compiler and load; see the text before quoting them.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>

struct Xoshiro256ss {                       // same generator as Listing 2.2, condensed
    std::uint64_t s[4];
    static std::uint64_t rotl(std::uint64_t v, int k) { return (v << k) | (v >> (64 - k)); }
    explicit Xoshiro256ss(std::uint64_t seed) {
        for (auto& w : s) {
            std::uint64_t z = (seed += 0x9e3779b97f4a7c15ULL);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            w = z ^ (z >> 31);
        }
    }
    std::uint64_t operator()() {
        const std::uint64_t r = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t;    s[3] = rotl(s[3], 45);
        return r;
    }
    using result_type = std::uint64_t;
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return ~0ULL; }
};

volatile std::uint64_t sink;                 // stops the compiler deleting the loops

template <class F>
void bench(const char* name, F f) {
    const long long N = 40'000'000;
    std::uint64_t acc = 0;
    for (int i = 0; i < 1000; ++i) acc ^= f();   // warm-up
    auto t0 = std::chrono::steady_clock::now();
    for (long long i = 0; i < N; ++i) acc += f();
    auto t1 = std::chrono::steady_clock::now();
    sink = acc;
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
    std::printf("  %-44s %6.2f ns/call\n", name, ns);
}

int main() {
    std::printf("Raw generators:\n");
    std::srand(1);
    bench("std::rand() (glibc)", [] { return std::uint64_t(std::rand()); });
    std::minstd_rand lcg(1);
    bench("std::minstd_rand", [&] { return std::uint64_t(lcg()); });
    std::mt19937 mt(1);
    bench("std::mt19937", [&] { return std::uint64_t(mt()); });
    std::mt19937_64 mt64(1);
    bench("std::mt19937_64", [&] { return mt64(); });
    Xoshiro256ss xo(1);
    bench("xoshiro256**", [&] { return xo(); });

    std::printf("Die roll in [1,6]:\n");
    bench("rand() % 6 + 1", [] { return std::uint64_t(std::rand() % 6 + 1); });
    std::uniform_int_distribution<int> d6(1, 6);
    bench("uniform_int_distribution + mt19937_64", [&] { return std::uint64_t(d6(mt64)); });
    bench("uniform_int_distribution + xoshiro256**", [&] { return std::uint64_t(d6(xo)); });
    bench("xoshiro256** + multiply-shift", [&] {
        return std::uint64_t((static_cast<unsigned __int128>(xo()) * 6) >> 64) + 1;
    });
}
