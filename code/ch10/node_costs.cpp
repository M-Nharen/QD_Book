// Heap allocations and memory footprint of the standard containers (Sections 10.2-10.4).
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <list>
#include <malloc.h>
#include <map>
#include <new>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---- A counting global allocator (replacing operator new/delete is allowed by the
// standard). malloc_usable_size gives the size glibc really reserved for each block.
static bool g_on = false;
static long g_calls = 0, g_live_blocks = 0, g_live_bytes = 0;
static std::size_t g_last_request = 0;

[[gnu::noinline]] void* operator new(std::size_t n) {
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc{};
    if (g_on) {
        ++g_calls; ++g_live_blocks; g_last_request = n;
        g_live_bytes += static_cast<long>(malloc_usable_size(p)) + 8;  // + chunk header
    }
    return p;
}
[[gnu::noinline]] void operator delete(void* p) noexcept {
    if (p && g_on) { --g_live_blocks;
                     g_live_bytes -= static_cast<long>(malloc_usable_size(p)) + 8; }
    std::free(p);
}
[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept {
    operator delete(p);
}

template <class C, class F>
void measure(const char* name, int n, F&& insert) {
    g_calls = g_live_blocks = g_live_bytes = 0;
    g_on = true;
    {
        C c;
        for (int i = 0; i < n; ++i) insert(c, i);
        std::printf("%-26s allocs=%6ld  live blocks=%6ld  bytes/elem=%6.1f  last req=%zu\n",
                    name, g_calls, g_live_blocks, double(g_live_bytes) / n, g_last_request);
    }
    g_on = false;
}

int main() {
    const int n = 10'000;
    std::printf("%d int elements (int->int for maps); libstdc++, glibc malloc\n", n);
    measure<std::vector<int>>("vector<int>", n, [](auto& c, int i) { c.push_back(i); });
    measure<std::deque<int>>("deque<int>", n, [](auto& c, int i) { c.push_back(i); });
    measure<std::list<int>>("list<int>", n, [](auto& c, int i) { c.push_back(i); });
    measure<std::set<int>>("set<int>", n, [](auto& c, int i) { c.insert(i); });
    measure<std::map<int, int>>("map<int,int>", n, [](auto& c, int i) { c.emplace(i, i); });
    measure<std::unordered_set<int>>("unordered_set<int>", n,
                                     [](auto& c, int i) { c.insert(i); });
    measure<std::unordered_map<int, int>>("unordered_map<int,int>", n,
                                          [](auto& c, int i) { c.emplace(i, i); });
    measure<std::unordered_map<std::string, int>>("unordered_map<string,int>", n,
        [](auto& c, int i) { c.emplace(std::to_string(i), i); });   // short: no SSO spill
    std::printf("sizeof: vector=%zu deque=%zu list=%zu map=%zu unordered_map=%zu\n",
                sizeof(std::vector<int>), sizeof(std::deque<int>), sizeof(std::list<int>),
                sizeof(std::map<int, int>), sizeof(std::unordered_map<int, int>));
}
