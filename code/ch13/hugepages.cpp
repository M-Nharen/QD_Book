// Getting and verifying huge pages (Section 13.3). Tries hugetlbfs pages (MAP_HUGETLB),
// then transparent huge pages (madvise), confirms what the kernel actually did from
// /proc/self/smaps, and times every individual first-touch fault for both page sizes.
#include <sys/mman.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

constexpr std::size_t kSmall = 4096, kHuge = 2u << 20, kBytes = 64u << 20;

static long smaps_kb(const void* addr, const char* field) {  // field of the VMA at addr
    std::ifstream f("/proc/self/smaps");
    const auto a = reinterpret_cast<std::uintptr_t>(addr);
    bool in = false;
    for (std::string line; std::getline(f, line);) {
        unsigned long lo, hi;
        if (std::sscanf(line.c_str(), "%lx-%lx ", &lo, &hi) == 2)
            in = (a >= lo && a < hi);
        else if (in && line.rfind(field, 0) == 0)
            return std::strtol(line.c_str() + std::strlen(field) + 1, nullptr, 10);
    }
    return -1;
}

static unsigned char* aligned_map(std::size_t bytes) {   // 2 MiB-aligned anonymous range
    void* p = mmap(nullptr, bytes + kHuge, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { std::perror("mmap"); std::exit(1); }
    auto a = (reinterpret_cast<std::uintptr_t>(p) + kHuge - 1) & ~(kHuge - 1);
    return reinterpret_cast<unsigned char*>(a);
}

static void time_faults(const char* name, unsigned char* m, std::size_t step) {
    std::vector<double> us;                        // one sample per first-touch fault
    for (std::size_t off = 0; off < kBytes; off += step) {
        const auto t0 = std::chrono::steady_clock::now();
        m[off] = 1;                                // first write: page fault
        const auto t1 = std::chrono::steady_clock::now();
        us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }
    std::sort(us.begin(), us.end());
    double total = 0;
    for (double x : us) total += x;
    std::printf("%-10s %6zu faults  median %7.2f us  max %7.1f us  total %6.1f ms\n",
                name, us.size(), us[us.size() / 2], us.back(), total / 1e3);
}

int main() {
    // 1. hugetlbfs: needs pages reserved in advance (vm.nr_hugepages).
    void* h = mmap(nullptr, kHuge, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (h == MAP_FAILED)
        std::printf("MAP_HUGETLB: failed (%s) - no pages reserved in the pool\n",
                    std::strerror(errno));
    else
        std::printf("MAP_HUGETLB: got a 2 MiB page from the reserved pool\n");

    // 2. Transparent huge pages, requested per range with madvise.
    unsigned char* thp = aligned_map(kBytes);
    madvise(thp, kBytes, MADV_HUGEPAGE);
    unsigned char* small = aligned_map(kBytes);
    madvise(small, kBytes, MADV_NOHUGEPAGE);
    time_faults("4 KiB", small, kSmall);
    time_faults("2 MiB THP", thp, kHuge);
    std::printf("smaps AnonHugePages: THP range %ld kB, 4 KiB range %ld kB\n",
                smaps_kb(thp, "AnonHugePages"), smaps_kb(small, "AnonHugePages"));

    // 3. Unmapping one 4 KiB page inside a huge page splits the mapping.
    munmap(thp + kHuge + kSmall, kSmall);
    std::printf("after punching a 4 KiB hole: AnonHugePages %ld kB in the first part\n",
                smaps_kb(thp, "AnonHugePages"));
}
