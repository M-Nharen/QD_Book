// The cost of first touch: page faults, MAP_POPULATE, mlock, transparent huge pages
// (Section 7.4). Timings are machine-dependent; fault counts are the robust part.
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <sys/resource.h>

constexpr std::size_t kPage = 4096;
constexpr std::size_t kBytes = 64u << 20;          // 64 MiB = 16384 pages of 4 KiB

long minor_faults() {
    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_minflt;
}

// Write one byte per 4 KiB page; report wall time and minor page faults taken.
void touch(const char* label, char* p, std::size_t bytes) {
    const long f0 = minor_faults();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < bytes; i += kPage) p[i] = 1;
    const auto t1 = std::chrono::steady_clock::now();
    const long faults = minor_faults() - f0;
    const double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    std::printf("  %-40s %6ld faults  %9.0f us  (%6.0f ns per page)\n", label, faults, us,
                us * 1000.0 / double(bytes / kPage));
}

char* map(std::size_t bytes, int extra_flags) {
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | extra_flags, -1, 0);
    return p == MAP_FAILED ? nullptr : static_cast<char*>(p);
}

int main() {
    std::printf("64 MiB anonymous mapping, one write per 4 KiB page:\n");
    char* a = map(kBytes, 0);
    touch("first touch (demand paging)", a, kBytes);
    touch("second touch (pages already mapped)", a, kBytes);
    madvise(a, kBytes, MADV_DONTNEED);             // give the pages back to the kernel
    touch("after MADV_DONTNEED", a, kBytes);
    munmap(a, kBytes);

    const long f0 = minor_faults();
    const auto t0 = std::chrono::steady_clock::now();
    char* b = map(kBytes, MAP_POPULATE);           // kernel populates at mmap time
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("  %-40s %6ld faults  %9.0f us  (cost moved into mmap)\n",
                "mmap(MAP_POPULATE) call itself", minor_faults() - f0,
                std::chrono::duration<double, std::micro>(t1 - t0).count());
    touch("first touch after MAP_POPULATE", b, kBytes);
    munmap(b, kBytes);

    // Transparent huge pages: map 2 MiB extra so we can align the start to 2 MiB.
    constexpr std::size_t kHuge = 2u << 20;
    char* raw = map(kBytes + kHuge, 0);
    char* c = reinterpret_cast<char*>(
        (reinterpret_cast<std::uintptr_t>(raw) + kHuge - 1) & ~(kHuge - 1));
    const int rc = madvise(c, kBytes, MADV_HUGEPAGE);
    std::printf("  madvise(MADV_HUGEPAGE): %s\n", rc == 0 ? "ok" : std::strerror(errno));
    touch("first touch, THP requested", c, kBytes);
    munmap(raw, kBytes + kHuge);

    // mlock: fault in and pin. Limited by RLIMIT_MEMLOCK for unprivileged processes.
    rlimit rl{};
    getrlimit(RLIMIT_MEMLOCK, &rl);
    const std::size_t lock_bytes = 4u << 20;
    char* d = map(lock_bytes, 0);
    std::printf("  RLIMIT_MEMLOCK = %lu KiB; mlock(4 MiB): ",
                static_cast<unsigned long>(rl.rlim_cur / 1024));
    if (mlock(d, lock_bytes) == 0) {
        std::printf("ok\n");
        touch("first touch after mlock", d, lock_bytes);
        munlock(d, lock_bytes);
    } else {
        std::printf("failed: %s\n", std::strerror(errno));
    }
    munmap(d, lock_bytes);
}
