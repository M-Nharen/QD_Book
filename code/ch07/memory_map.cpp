// Where do objects live? Classify addresses using /proc/self/maps (Section 7.1).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>

int g_init = 42;                 // .data   (initialized, writable)
int g_zero;                      // .bss    (zero-initialized, takes no space in the file)
const char* g_lit = "literal";   // the pointer is in .data; the characters are in .rodata

// Find the /proc/self/maps line whose [lo, hi) range contains p.
void where(const char* what, const void* p) {
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    std::FILE* f = std::fopen("/proc/self/maps", "r");
    char line[512];
    while (f && std::fgets(line, sizeof line, f)) {
        unsigned long lo = 0, hi = 0;
        char perms[8] = {};
        int name_at = 0;
        if (std::sscanf(line, "%lx-%lx %7s %*s %*s %*s %n", &lo, &hi, perms, &name_at) < 3)
            continue;
        if (a < lo || a >= hi) continue;
        line[std::strcspn(line, "\n")] = '\0';
        const char* name = line + name_at;
        const char* slash = std::strrchr(name, '/');
        std::printf("  %-28s %s  %-24s (region %lu KiB)\n", what, perms,
                    *name ? (slash ? slash + 1 : name) : "(anonymous mmap)",
                    (hi - lo) / 1024);
        break;
    }
    if (f) std::fclose(f);
}

[[gnu::noinline]] std::uintptr_t frame_addr(int depth) {
    volatile char local = 0;                       // volatile: must live in this frame
    if (depth == 0) return reinterpret_cast<std::uintptr_t>(&local);
    std::uintptr_t r = frame_addr(depth - 1);
    return r + local;                              // use after the call: no tail call
}

int main() {
    int local = 1;
    void* small = std::malloc(64);                 // below the mmap threshold
    void* large = std::malloc(1 << 20);            // 1 MiB: above the default threshold
    std::printf("Address -> mapping (addresses change every run because of ASLR):\n");
    where("main (code)", reinterpret_cast<const void*>(&main));
    where("string literal", g_lit);
    where("g_init (.data)", &g_init);
    where("g_zero (.bss)", &g_zero);
    where("malloc(64)", small);
    where("malloc(1 MiB)", large);
    where("local variable", &local);

    const std::uintptr_t a = frame_addr(0), b = frame_addr(1);
    std::printf("one extra frame of frame_addr moves the stack pointer by %ld bytes "
                "(stack grows %s)\n", static_cast<long>(a > b ? a - b : b - a),
                b < a ? "down" : "up");
    rlimit rl{};
    getrlimit(RLIMIT_STACK, &rl);
    std::printf("main-thread stack limit (RLIMIT_STACK): %lu KiB\n",
                static_cast<unsigned long>(rl.rlim_cur / 1024));
    std::free(small);
    std::free(large);
    return g_init + g_zero == 42 ? 0 : 1;
}
