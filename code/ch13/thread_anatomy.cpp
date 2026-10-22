// book: flags=-pthread
// Processes and threads on Linux (Section 13.1). Also run under ThreadSanitizer (tsan).
// Every thread of a process shares one address space and one PID but has its own TID,
// stack and thread-local storage block (found through the fs segment base on x86-64).
#include <asm/prctl.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

thread_local long tls_counter = 0;                 // one copy per thread
std::atomic<int> started{0};
std::atomic<bool> release{false};

static std::uintptr_t fs_base() {                  // the thread pointer (TCB address)
    unsigned long v = 0;
    syscall(SYS_arch_prctl, ARCH_GET_FS, &v);
    return v;
}

static int threads_in_status() {                   // "Threads:" line of /proc/self/status
    std::ifstream f("/proc/self/status");
    for (std::string line; std::getline(f, line);)
        if (line.rfind("Threads:", 0) == 0) return std::stoi(line.substr(8));
    return -1;
}

static std::string describe(const char* who) {
    int local = 0;                                 // lives on this thread's stack
    pthread_attr_t a;
    pthread_getattr_np(pthread_self(), &a);
    void* lo = nullptr;
    std::size_t size = 0, guard = 0;
    pthread_attr_getstack(&a, &lo, &size);
    pthread_attr_getguardsize(&a, &guard);
    pthread_attr_destroy(&a);
    const auto fs = fs_base();
    const auto tls = reinterpret_cast<std::uintptr_t>(&tls_counter);
    char buf[256];
    std::snprintf(buf, sizeof buf,
                  "%-5s pid %d tid %ld  stack %p (%zu KiB, guard %zu KiB)\n"
                  "      &local %p  fs base %#lx  &tls - fs = %ld\n",
                  who, getpid(), syscall(SYS_gettid), lo, size >> 10, guard >> 10,
                  static_cast<void*>(&local), static_cast<unsigned long>(fs),
                  static_cast<long>(tls - fs));
    return buf;
}

int main() {
    std::string report[3];
    report[0] = describe("main");
    std::vector<std::thread> ts;
    for (int i = 1; i <= 2; ++i)
        ts.emplace_back([i, &report] {
            ++started;
            while (started.load() < 2) {}          // both helper threads now exist
            char name[8];
            std::snprintf(name, sizeof name, "T%d", i);
            report[i] = describe(name);            // each thread writes its own slot
            while (!release.load()) {}             // stay alive until main has looked
        });
    while (started.load() < 2) {}
    const int alive = threads_in_status();         // read while all three are running
    release = true;
    for (auto& t : ts) t.join();                   // join: happens-before for report[]
    for (auto& r : report) std::fputs(r.c_str(), stdout);
    std::printf("threads alive (from /proc/self/status): %d\n", alive);

    // Cost of creating and joining a thread that does nothing.
    constexpr int kN = 2000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kN; ++i) std::thread([] {}).join();
    const auto t1 = std::chrono::steady_clock::now();
    std::printf("std::thread create+join: %.1f us each (mean of %d)\n",
                std::chrono::duration<double, std::micro>(t1 - t0).count() / kN, kN);
}
