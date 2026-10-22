// SIMD basics (Section 12.6): the same work done one element at a time and 16 or 32 bytes
// at a time with intrinsics. SSE2 is part of baseline x86-64; the AVX2 path is compiled
// for AVX2 with a function attribute and only called if the CPU reports AVX2.
#include <immintrin.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

// ---- count bytes below a threshold (for example: price levels inside a band) ----
[[gnu::noipa]] std::size_t count_below_scalar(const std::uint8_t* a, std::size_t n,
                                              std::uint8_t k) {
    std::size_t c = 0;
    for (std::size_t i = 0; i < n; ++i) c += (a[i] < k);
    return c;
}

[[gnu::noipa]] std::size_t count_below_sse2(const std::uint8_t* a, std::size_t n,
                                            std::uint8_t k) {
    const __m128i flip = _mm_set1_epi8(char(0x80));   // unsigned -> signed order trick
    const __m128i kv = _mm_xor_si128(_mm_set1_epi8(char(k)), flip);
    std::size_t c = 0, i = 0;
    while (i + 16 <= n) {
        __m128i acc = _mm_setzero_si128();             // 16 byte-wide counters
        const std::size_t stop = std::min(n - 15, i + 255 * 16);  // byte counters <= 255
        for (; i < stop; i += 16) {
            __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
            __m128i lt = _mm_cmplt_epi8(_mm_xor_si128(x, flip), kv);  // 0xFF where a < k
            acc = _mm_sub_epi8(acc, lt);               // subtracting -1 adds 1
        }
        __m128i sums = _mm_sad_epu8(acc, _mm_setzero_si128());  // two 64-bit lane sums
        c += std::size_t(_mm_cvtsi128_si64(sums)) +
             std::size_t(_mm_cvtsi128_si64(_mm_unpackhi_epi64(sums, sums)));
    }
    for (; i < n; ++i) c += (a[i] < k);                 // tail
    return c;
}

// ---- sum of floats ----
[[gnu::noipa]] float sum_scalar(const float* a, std::size_t n) {
    float s = 0;
    for (std::size_t i = 0; i < n; ++i) s += a[i];     // strict left-to-right order
    return s;
}

[[gnu::target("avx2"), gnu::noipa]] float sum_avx2(const float* a, std::size_t n) {
    __m256 s0 = _mm256_setzero_ps(), s1 = s0, s2 = s0, s3 = s0;   // 4 x 8 lanes
    std::size_t i = 0;
    for (; i + 32 <= n; i += 32) {
        s0 = _mm256_add_ps(s0, _mm256_loadu_ps(a + i));
        s1 = _mm256_add_ps(s1, _mm256_loadu_ps(a + i + 8));
        s2 = _mm256_add_ps(s2, _mm256_loadu_ps(a + i + 16));
        s3 = _mm256_add_ps(s3, _mm256_loadu_ps(a + i + 24));
    }
    __m256 s = _mm256_add_ps(_mm256_add_ps(s0, s1), _mm256_add_ps(s2, s3));
    __m128 h = _mm_add_ps(_mm256_castps256_ps128(s), _mm256_extractf128_ps(s, 1));
    h = _mm_add_ps(h, _mm_movehl_ps(h, h));
    h = _mm_add_ss(h, _mm_shuffle_ps(h, h, 1));
    float r = _mm_cvtss_f32(h);
    for (; i < n; ++i) r += a[i];
    return r;
}

template <class F>
double best_ns(F&& f, double elems) {
    double best = 1e300;
    for (int t = 0; t < 9; ++t) {
        auto t0 = std::chrono::steady_clock::now();
        for (int r = 0; r < 200; ++r) f();
        auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count());
    }
    return best / (200.0 * elems);
}

int main() {
    std::mt19937 rng(5);
    std::vector<std::uint8_t> bytes(1 << 15);           // 32 KiB
    for (auto& b : bytes) b = std::uint8_t(rng());
    std::vector<float> fl(1 << 13);                      // 32 KiB
    for (auto& f : fl) f = float(rng() % 1000) * 0.01f;

    volatile std::size_t cs = 0;
    volatile float fs = 0;
    const std::size_t c1 = count_below_scalar(bytes.data(), bytes.size(), 100);
    const std::size_t c2 = count_below_sse2(bytes.data(), bytes.size(), 100);
    std::printf("count_below: scalar %zu, sse2 %zu\n", c1, c2);
    std::printf("  scalar %.3f ns/byte\n", best_ns([&] {
        cs = count_below_scalar(bytes.data(), bytes.size(), 100); }, double(bytes.size())));
    std::printf("  sse2   %.3f ns/byte\n", best_ns([&] {
        cs = count_below_sse2(bytes.data(), bytes.size(), 100); }, double(bytes.size())));

    const float r1 = sum_scalar(fl.data(), fl.size());
    std::printf("float sum: scalar %.4f\n", r1);
    std::printf("  scalar %.3f ns/float\n",
                best_ns([&] { fs = sum_scalar(fl.data(), fl.size()); }, double(fl.size())));
    if (__builtin_cpu_supports("avx2")) {
        const float r2 = sum_avx2(fl.data(), fl.size());
        std::printf("  avx2   %.3f ns/float   (result %.4f, relative difference %.1e)\n",
                    best_ns([&] { fs = sum_avx2(fl.data(), fl.size()); },
                            double(fl.size())),
                    r2, std::fabs(r2 - r1) / r1);
    } else {
        std::printf("  avx2   not supported on this CPU\n");
    }
}
