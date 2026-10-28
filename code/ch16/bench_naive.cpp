// Showdown, naive side (Section 16.7): the benchmark most people write first. One input,
// a loop of 10 million calls, the result thrown away, the total time divided by N.
#include <chrono>
#include <cstdio>
#include "parsers.hpp"

int main() {
    constexpr int kN = 10'000'000;
    using clk = std::chrono::steady_clock;
    const auto t0 = clk::now();
    for (int i = 0; i < kN; ++i) parse_strtod("101.2575");
    const auto t1 = clk::now();
    for (int i = 0; i < kN; ++i) parse_fixed("101.2575");
    const auto t2 = clk::now();
    const double a = std::chrono::duration<double, std::nano>(t1 - t0).count() / kN;
    const double b = std::chrono::duration<double, std::nano>(t2 - t1).count() / kN;
    std::printf("parse_strtod %9.4f ns/call\nparse_fixed  %9.4f ns/call\n"
                "speed-up     %9.0fx\n", a, b, a / b);
}
