// Checks the numeric answers to the Chapter 13 practice problems, and tests the CPU-list
// parser and formatter of P13.8 against each other on random sets.
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// ---- P13.8
std::vector<int> parse_cpu_list(std::string_view s) {
    std::vector<int> out;
    if (s.empty()) return out;                     // "" is the empty set (e.g. "isolated")
    auto number = [](std::string_view t) {
        int v = 0;
        auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
        if (t.empty() || ec != std::errc{} || p != t.data() + t.size() || v < 0)
            throw std::invalid_argument("bad cpu number");
        return v;
    };
    while (true) {
        const auto comma = s.find(',');
        const std::string_view item = s.substr(0, comma);
        const auto dash = item.find('-');
        if (dash == std::string_view::npos) {
            out.push_back(number(item));
        } else {
            const int lo = number(item.substr(0, dash)), hi = number(item.substr(dash + 1));
            if (lo > hi) throw std::invalid_argument("reversed range");
            for (int c = lo; c <= hi; ++c) out.push_back(c);
        }
        if (comma == std::string_view::npos) break;
        s.remove_prefix(comma + 1);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::string format_cpu_list(std::vector<int> v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    std::string out;
    for (std::size_t i = 0; i < v.size();) {
        std::size_t j = i;
        while (j + 1 < v.size() && v[j + 1] == v[j] + 1) ++j;
        if (!out.empty()) out += ',';
        out += std::to_string(v[i]);
        if (j > i) out += '-' + std::to_string(v[j]);
        i = j + 1;
    }
    return out;
}

static bool rejects(std::string_view s) {
    try { parse_cpu_list(s); } catch (const std::invalid_argument&) { return true; }
    return false;
}

int main() {
    // P13.3: 1 GiB touched with 4 KiB, 2 MiB and 1 GiB pages.
    const double GiB = 1024.0 * 1024 * 1024;
    const double p4k = GiB / 4096, p2m = GiB / (2 << 20);
    std::printf("P13.3  4 KiB: %.0f faults, %.0f PTEs, %.0f KiB of last-level tables\n",
                p4k, p4k, p4k / 512 * 4);
    std::printf("       2 MiB: %.0f faults, %.0f PMD entries, %.0f KiB of tables\n", p2m,
                p2m, p2m / 512 * 4);
    std::printf("       1 GiB: 1 fault, 1 PUD entry (inside one 4 KiB table page)\n");

    // P13.5: probability that a tick lands inside a 5 us handler, exact and simulated.
    std::mt19937_64 rng(13);
    for (double period_us : {4000.0, 1000.0, 1e6}) {
        std::uniform_real_distribution<double> phase(0, period_us);
        long hit = 0, n = 20'000'000;
        for (long i = 0; i < n; ++i) hit += phase(rng) < 5.0;   // tick within [t, t+5)
        std::printf("P13.5  tick every %7.0f us: exact %.6f%%  simulated %.6f%%\n",
                    period_us, 100 * 5.0 / period_us, 100.0 * double(hit) / double(n));
    }

    // P13.6: distinct pages written by 10^7 uniform updates (copy-on-write faults).
    const double updates = 1e7;
    double faults4k = 0;
    for (double page : {4096.0, 2097152.0}) {
        const double pages = 4 * GiB / page;
        const double distinct = pages * (1 - std::pow(1 - 1 / pages, updates));
        if (page == 4096.0) faults4k = distinct;
        std::printf("P13.6  %4.0f KiB pages: %7.0f pages, COW faults %9.1f (%.5f), "
                    "extra %.3f GiB\n", page / 1024, pages, distinct, distinct / pages,
                    distinct * page / GiB);
    }
    double h2048 = 0;
    for (int k = 1; k <= 2048; ++k) h2048 += 1.0 / k;
    std::printf("       2 MiB: expected updates until all copied %.0f (%.1f ms at 1e6/s)\n",
                2048 * h2048, 2048 * h2048 / 1e3);
    std::printf("       4 KiB: fraction of the 1e7 updates that fault %.4f; "
                "fault time at 2.4 us each %.2f s\n", faults4k / updates,
                faults4k * 2.4e-6);

    // P13.7: cost of condition-variable handoffs at 200,000 messages per second.
    for (double cpu_us : {1.1, 1.6})
        std::printf("P13.7  %.1f us of CPU per handoff x 200000/s = %.2f core\n", cpu_us,
                    cpu_us * 200000 / 1e6);

    // P13.9: dependent lookups, local vs remote DRAM.
    std::printf("P13.9  remote %.2f s/s, local %.2f s/s, saved %.2f s/s\n",
                2e6 * 150e-9, 2e6 * 90e-9, 2e6 * 60e-9);

    // P13.10: throttling budget.
    std::printf("P13.10 RT share %.0f%%, lost %.0f%%, longest gap %.0f ms\n",
                100.0 * 950000 / 1000000, 100.0 * 50000 / 1000000, 50000 / 1e3);

    // P13.8: tests.
    int bad = 0;
    bad += parse_cpu_list("0-3,8,10-11") != std::vector<int>{0, 1, 2, 3, 8, 10, 11};
    bad += format_cpu_list({11, 10, 8, 3, 2, 1, 0}) != "0-3,8,10-11";
    bad += format_cpu_list({5}) != "5" || format_cpu_list({}) != "";
    for (const char* s : {",", "1,", "3-1", "a", "1-", "-2", "1--2", "2 ,3"})
        bad += !rejects(s);
    std::uniform_int_distribution<int> coin(0, 2);
    for (int t = 0; t < 20000; ++t) {
        std::vector<int> v;
        for (int c = 0; c < 64; ++c)
            if (coin(rng) == 0) v.push_back(c);
        const std::string f = format_cpu_list(v);
        bad += parse_cpu_list(f) != v;
        bad += format_cpu_list(parse_cpu_list(f)) != f;
    }
    std::printf("P13.8  parser/formatter tests: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
