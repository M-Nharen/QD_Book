// What the kernel does on a page fault (Section 13.2): the shared zero page, write
// faults, copy-on-write after fork, and the cost of fork itself. 4 KiB pages are forced
// with MADV_NOHUGEPAGE so that each fault is one page.
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>

constexpr std::size_t kPage = 4096, kBytes = 64u << 20, kPages = kBytes / kPage;

static long minflt() {
    rusage r{};
    getrusage(RUSAGE_SELF, &r);
    return r.ru_minflt;
}
static double now_us() {
    using namespace std::chrono;
    return duration<double, std::micro>(steady_clock::now().time_since_epoch()).count();
}

template <class F> static void phase(const char* what, F&& touch) {
    const long f0 = minflt();
    const double t0 = now_us();
    touch();
    const double t1 = now_us();
    const long faults = minflt() - f0;
    std::printf("%-34s %6ld faults %9.0f us  %7.0f ns/page\n", what, faults, t1 - t0,
                (t1 - t0) * 1e3 / kPages);
}

static unsigned char* map(std::size_t bytes) {
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
    if (p == MAP_FAILED) { std::perror("mmap"); std::exit(1); }
    madvise(p, bytes, MADV_NOHUGEPAGE);
    return static_cast<unsigned char*>(p);
}

int main() {
    unsigned char* a = map(kBytes);
    volatile unsigned sink = 0;
    (void)now_us();                                // first clock read faults in vDSO data
    std::printf("64 MiB anonymous mapping = %zu pages of 4 KiB\n", kPages);
    phase("read every page (never written)", [&] {
        for (std::size_t i = 0; i < kBytes; i += kPage) sink = sink + a[i];
    });
    phase("then write every page", [&] {
        for (std::size_t i = 0; i < kBytes; i += kPage) a[i] = 1;
    });
    phase("write every page again", [&] {
        for (std::size_t i = 0; i < kBytes; i += kPage) a[i] = 2;
    });

    std::fflush(stdout);                           // do not duplicate buffered output
    const double tf = now_us();
    const pid_t pid = fork();
    if (pid == 0) {                                // child: same addresses, shared pages
        const double t_child = now_us() - tf;
        phase("child: read every page", [&] {
            for (std::size_t i = 0; i < kBytes; i += kPage) sink = sink + a[i];
        });
        phase("child: write every page (COW copy)", [&] {
            for (std::size_t i = 0; i < kBytes; i += kPage) a[i] = 3;
        });
        std::printf("child: saw a[0] = %d; fork() returned after about %.0f us\n",
                    a[0], t_child);
        std::fflush(stdout);
        _exit(0);
    }
    waitpid(pid, nullptr, 0);
    phase("parent, child gone: write again", [&] {
        for (std::size_t i = 0; i < kBytes; i += kPage) a[i] = 4;
    });
    std::printf("parent: a[0] = %d (the child's writes went to its own copies)\n", a[0]);

    // fork() copies page tables, so its cost grows with the touched address space.
    for (std::size_t mib : {0, 64, 512}) {
        unsigned char* b = mib ? map(mib << 20) : nullptr;
        for (std::size_t i = 0; i < (mib << 20); i += kPage) b[i] = 1;
        constexpr int kForks = 20;
        const double t0 = now_us();
        for (int k = 0; k < kForks; ++k) {
            const pid_t c = fork();
            if (c == 0) _exit(0);
            waitpid(c, nullptr, 0);
        }
        std::printf("fork+exit+wait with %3zu MiB more touched: %7.0f us\n", mib,
                    (now_us() - t0) / kForks);
        if (b) munmap(b, mib << 20);
    }
}
