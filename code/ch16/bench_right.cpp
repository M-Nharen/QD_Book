// Showdown, corrected side (Section 16.7): check correctness first, use varied realistic
// inputs, keep every result alive, warm up, pin, time batches with the fenced TSC,
// subtract the timer's own cost, and report a distribution instead of a mean.
#include <cstdio>
#include <random>
#include <string>
#include <vector>
#include "bench.hpp"
#include "parsers.hpp"

int main() {
    std::mt19937_64 rng(16);                           // prices like "7.5", "10342.0125"
    std::vector<std::string> inputs(4096);
    for (auto& s : inputs) {
        s = std::to_string(1 + rng() % 20000);
        if (const int d = int(rng() % 5))              // 0-4 decimals
            s += "." + std::to_string(rng() % 10000).substr(0, d);
    }
    for (const auto& s : inputs)                       // 1. correct before fast
        if (parse_fixed(s.c_str()) != parse_strtod(s.c_str())) {
            std::printf("MISMATCH on %s\n", s.c_str());
            return 1;
        }
    bench::pin(1);
    const double tpn = bench::ticks_per_ns();
    constexpr int kBatch = 32, kBatches = 20000;

    auto measure = [&](auto parse) {
        std::vector<double> per_call(kBatches);
        std::size_t next = 0;
        for (int b = -kBatches / 10; b < kBatches; ++b) {      // first 10%: warm-up
            const std::uint64_t t0 = bench::start();
            for (int k = 0; k < kBatch; ++k)
                bench::do_not_optimize(parse(inputs[next++ & 4095].c_str()));
            const std::uint64_t t1 = bench::stop();
            if (b >= 0) per_call[b] = double(t1 - t0) / tpn / kBatch;
        }
        return per_call;
    };
    const auto empty = bench::summarize(measure([](const char* s) { return s[0]; }));
    std::printf("TSC %.3f GHz; loop + timer overhead %.2f ns/call (median), subtracted\n",
                tpn, empty.p50);
    std::printf("%-13s %8s %8s %8s %8s %8s\n", "ns per call", "mean", "p50", "p99",
                "p99.9", "max");
    const auto row = [&](const char* name, bench::Summary s) {
        const double o = empty.p50;
        std::printf("%-13s %8.2f %8.2f %8.2f %8.2f %8.1f\n", name, s.mean - o, s.p50 - o,
                    s.p99 - o, s.p999 - o, s.max - o);
    };
    row("parse_strtod", bench::summarize(measure(parse_strtod)));
    row("parse_fixed", bench::summarize(measure(parse_fixed)));
}
