// System calls versus the vDSO (Section 13.5). Each operation is timed in a loop and the
// mean cost per call is printed. The same question ("what time is it?", "which CPU am
// I on?") is answered with and without entering the kernel.
#include <fcntl.h>
#include <sched.h>
#include <sys/auxv.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <x86intrin.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>

constexpr int kN = 200000;
volatile long sink;

template <class F> static void bench(const char* what, F&& f) {
    for (int i = 0; i < 1000; ++i) f();            // warm up
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kN; ++i) f();
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("%-46s %7.1f ns\n", what,
                std::chrono::duration<double, std::nano>(t1 - t0).count() / kN);
}

int main() {
    std::ifstream maps("/proc/self/maps");
    for (std::string line; std::getline(maps, line);)
        if (line.find("[vdso]") != std::string::npos) std::printf("%s\n", line.c_str());
    std::printf("AT_SYSINFO_EHDR (vDSO ELF header) = %#lx\n", getauxval(AT_SYSINFO_EHDR));
    timespec res{};
    clock_getres(CLOCK_MONOTONIC_COARSE, &res);
    std::printf("resolution of MONOTONIC_COARSE = %.3f ms (one timer tick)\n\n",
                res.tv_nsec / 1e6);

    timespec ts{};
    unsigned cpu = 0, node = 0;
    const int devnull = open("/dev/null", O_WRONLY);
    bench("rdtsc (no kernel, no library)", [] { sink = static_cast<long>(__rdtsc()); });
    bench("clock_gettime(MONOTONIC)          [vDSO]", [&] {
        clock_gettime(CLOCK_MONOTONIC, &ts); sink = ts.tv_nsec; });
    bench("clock_gettime(MONOTONIC_COARSE)   [vDSO]", [&] {
        clock_gettime(CLOCK_MONOTONIC_COARSE, &ts); sink = ts.tv_nsec; });
    bench("clock_gettime(MONOTONIC)       [syscall]", [&] {
        syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts); sink = ts.tv_nsec; });
    bench("clock_gettime(THREAD_CPUTIME_ID)", [&] {
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); sink = ts.tv_nsec; });
    bench("sched_getcpu()           [glibc: rseq area]", [] { sink = sched_getcpu(); });
    bench("getcpu                         [syscall]", [&] {
        syscall(SYS_getcpu, &cpu, &node, nullptr); sink = cpu; });
    bench("getppid()                      [syscall]", [] { sink = getppid(); });
    bench("write(/dev/null, 1 byte)       [syscall]", [&] {
        sink = write(devnull, "x", 1); });
    close(devnull);
}
