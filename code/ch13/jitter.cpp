// Measuring what the OS takes from a pinned, busy thread (Section 13.6). The thread spins
// reading the TSC; any gap between consecutive reads longer than 1 us is time the CPU
// spent elsewhere (interrupt, timer tick, another task, the hypervisor). The gaps are
// compared with the interrupts the (guest) kernel counted on this CPU.
#include <pthread.h>
#include <x86intrin.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

struct Irqs { long timer = 0, all = 0; };
static Irqs irq_counts(int cpu) {                  // this CPU's column of /proc/interrupts
    std::ifstream f("/proc/interrupts");
    std::string line;
    std::getline(f, line);                         // header: CPU0 CPU1 ...
    Irqs r;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string tag;
        long v = -1;
        in >> tag;
        for (int c = 0; c <= cpu && in >> v; ++c) {}
        if (!in) continue;                         // row without a column for this CPU
        r.all += v;
        if (tag == "LOC:") r.timer = v;
    }
    return r;
}

int main() {
    const int cpu = static_cast<int>(std::thread::hardware_concurrency()) - 1;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof set, &set);

    using Clock = std::chrono::steady_clock;       // calibrate TSC ticks per us (50 ms)
    const auto w0 = Clock::now();
    const std::uint64_t c0 = __rdtsc();
    while (Clock::now() - w0 < std::chrono::milliseconds(50)) {}
    const double us = std::chrono::duration<double, std::micro>(Clock::now() - w0).count();
    const double per_us = double(__rdtsc() - c0) / us;

    constexpr double kTop[] = {2, 5, 10, 20, 50, 100, 1000, 1e300};  // bucket limits, us
    long count[8] = {}, gaps = 0;
    double lost_us = 0, worst_us = 0;
    const Irqs i0 = irq_counts(cpu);
    const std::uint64_t start = __rdtsc(), run = std::uint64_t(per_us * 2e6);  // 2 s
    std::uint64_t prev = start;
    while (prev - start < run) {                   // the measured loop
        const std::uint64_t t = __rdtsc();
        if (t - prev > std::uint64_t(per_us)) {    // gap longer than 1 us
            const double gap = double(t - prev) / per_us;
            int b = 0;
            while (gap > kTop[b]) ++b;
            ++count[b], ++gaps;
            lost_us += gap;
            if (gap > worst_us) worst_us = gap;
        }
        prev = t;
    }
    const Irqs i1 = irq_counts(cpu);

    std::printf("CPU %d, 2 s, %.0f TSC ticks per us\n", cpu, per_us);
    for (int b = 0; b < 8; ++b)
        if (b < 7) std::printf("gaps %4.0f to %-4.0f us: %5ld\n", b ? kTop[b - 1] : 1.0,
                               kTop[b], count[b]);
        else std::printf("gaps over %-4.0f us:    %5ld\n", kTop[b - 1], count[b]);
    std::printf("total %ld gaps, %.1f ms lost (%.2f%%), worst %.0f us\n", gaps,
                lost_us / 1e3, lost_us / 2e4, worst_us);
    std::printf("interrupts the guest kernel counted on CPU %d: %ld (timer %ld)\n", cpu,
                i1.all - i0.all, i1.timer - i0.timer);
}
