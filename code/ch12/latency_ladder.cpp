// The latency hierarchy, measured by pointer chasing (Sections 12.2, 12.4, 12.7).
// Each 64-byte node holds the index of the next node. In "random" order every load depends
// on the previous one and its address is unpredictable, so the time per step is the load
// latency of whichever level holds the working set. "Sequential" visits the same nodes in
// address order, which the hardware prefetchers recognize.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

struct alignas(64) Node {                   // one node per cache line
    std::uint32_t next;
    std::uint32_t pad[15];
};
static_assert(sizeof(Node) == 64);

double chase_ns(const std::vector<Node>& nodes, std::size_t steps) {
    std::uint32_t i = 0;
    for (std::size_t s = 0; s < nodes.size(); ++s) i = nodes[i].next;     // warm up
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t s = 0; s < steps; ++s) i = nodes[i].next;            // dependent loads
    auto t1 = std::chrono::steady_clock::now();
    if (i == 0xffffffffu) std::puts("");    // keep the chain observable
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / double(steps);
}

// Link the nodes into one cycle that visits them in the given order.
void link(std::vector<Node>& nodes, const std::vector<std::uint32_t>& order) {
    for (std::size_t k = 0; k < order.size(); ++k)
        nodes[order[k]].next = order[(k + 1) % order.size()];
}

int main() {
    std::mt19937_64 rng(2026);
    std::printf("%10s %14s %14s\n", "working set", "random ns", "sequential ns");
    for (std::size_t bytes = 4 << 10; bytes <= (256u << 20); bytes *= 2) {
        const std::size_t n = bytes / sizeof(Node);
        std::vector<Node> nodes(n);
        std::vector<std::uint32_t> order(n);
        std::iota(order.begin(), order.end(), 0u);
        link(nodes, order);
        const std::size_t steps = 2'000'000;
        const double seq = chase_ns(nodes, steps);
        std::shuffle(order.begin() + 1, order.end(), rng);   // random single cycle
        link(nodes, order);
        const double rnd = chase_ns(nodes, steps);
        if (bytes < (1 << 20))
            std::printf("%7zu KiB %14.2f %14.2f\n", bytes >> 10, rnd, seq);
        else
            std::printf("%7zu MiB %14.2f %14.2f\n", bytes >> 20, rnd, seq);
    }
}
