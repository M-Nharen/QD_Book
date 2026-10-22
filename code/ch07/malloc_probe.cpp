// Observing glibc malloc from the outside: size classes, tcache reuse, the dynamic
// mmap threshold, brk growth, and per-thread arenas (Section 7.2).
// book: flags=-pthread
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <thread>
#include <unistd.h>

const char* mapping_of(const void* p) {           // "[heap]" or "anonymous" from maps
    static char out[64];
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    std::FILE* f = std::fopen("/proc/self/maps", "r");
    char line[512];
    std::strcpy(out, "?");
    while (f && std::fgets(line, sizeof line, f)) {
        unsigned long lo, hi;
        int at = 0;
        if (std::sscanf(line, "%lx-%lx %*s %*s %*s %*s %n", &lo, &hi, &at) < 2) continue;
        if (a >= lo && a < hi) {
            line[std::strcspn(line, "\n")] = '\0';
            std::snprintf(out, sizeof out, "%s", line[at] ? line + at : "anonymous mmap");
        }
    }
    if (f) std::fclose(f);
    return out;
}

int main() {
    std::printf("1) request -> usable bytes (malloc_usable_size)\n  ");
    for (std::size_t n : {1, 24, 25, 40, 41, 100, 1000, 1001}) {
        void* p = std::malloc(n);
        std::printf("%zu->%zu  ", n, malloc_usable_size(p));
        std::free(p);
    }

    std::printf("\n2) tcache: a freed chunk goes to the next same-class request\n");
    void* a = std::malloc(48);
    std::free(a);
    void* b = std::malloc(48);                     // same size class (64-byte chunk)
    void* c = std::malloc(40);                     // smaller class (48-byte chunk)
    std::printf("  malloc(48) after free: %s;  malloc(40): %s\n",
                a == b ? "same address" : "different", a == c ? "same" : "different");
    std::free(b);
    std::free(c);

    std::printf("3) dynamic mmap threshold (default 128 KiB)\n");
    char* brk0 = static_cast<char*>(sbrk(0));
    void* big = std::malloc(200 * 1024);
    std::printf("  1st malloc(200 KiB): mmapped chunks=%zu  region=%s\n",
                mallinfo2().hblks, mapping_of(big));
    std::free(big);                                // raises the threshold to ~200 KiB
    big = std::malloc(200 * 1024);
    std::printf("  2nd malloc(200 KiB): mmapped chunks=%zu  region=%s\n",
                mallinfo2().hblks, mapping_of(big));
    std::printf("  program break moved by %ld KiB\n",
                static_cast<long>((static_cast<char*>(sbrk(0)) - brk0) / 1024));
    std::free(big);

    std::printf("4) a second thread allocates from its own arena\n");
    void* mine = std::malloc(64);
    void* theirs = nullptr;
    std::thread t([&] { theirs = std::malloc(64); });
    t.join();
    std::printf("  main thread:  %s\n", mapping_of(mine));   // two calls: mapping_of
    std::printf("  other thread: %s\n", mapping_of(theirs)); // returns a static buffer
    std::free(theirs);                             // cross-thread free: goes to main's
    std::free(mine);                               // tcache (Section 7.2.4)
}
