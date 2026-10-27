// book: flags=-pthread
// Showdown, optimized side (Section 14.10): SPSC ring, both sides busy-poll. tsan-clean.
#include <immintrin.h>
#include <cstdio>
#include "showdown_harness.hpp"
#include "spsc_ring.hpp"

int main() {
    auto* ring = new SpscRing<Quote, 1024>;        // 32 KiB of slots
    std::printf("SpscRing<Quote, 1024>: busy-polling producer and consumer\n");
    run_showdown([&](const Quote& m) { while (!ring->try_push(m)) _mm_pause(); },
                 [&] {
                     Quote q;
                     while (!ring->try_pop(q)) { }
                     return q;
                 });
    delete ring;
}
