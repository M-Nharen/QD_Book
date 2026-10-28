// A tour of x86-64 assembly (Section 16.3). Each small function shows one pattern. main()
// disassembles this very executable with objdump and prints those functions, so what you
// see is exactly what g++ -O2 produced for the listing.
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct Order { std::int64_t px; std::int32_t qty, side; std::int64_t id; };   // 24 bytes

[[gnu::noinline]] std::int32_t qty_at(const Order* book, long i) { return book[i].qty; }

[[gnu::noinline]] unsigned div10(unsigned x) { return x / 10; }

[[gnu::noinline]] long best(long a, long b) { return a > b ? a : b; }

[[gnu::noinline]] int fee_bps(int venue) {
    switch (venue) {
        case 0: return 3;
        case 1: return 5;
        case 2: return 2;
        case 3: return 7;
        case 4: return 4;
        default: return 10;
    }
}

[[gnu::noinline]] long sum(const int* a, long n) {
    long s = 0;
    for (long i = 0; i < n; ++i) s += a[i];
    return s;
}

struct Venue { virtual ~Venue() = default; virtual int send(int qty) = 0; };

[[gnu::noinline]] int route(Venue& v, int qty) { return v.send(qty + 1); }

[[gnu::noinline]] void record(std::vector<int>& log, int x) { log.push_back(x); }

int main() {
    char exe[4096];
    const ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return 1;
    exe[n] = '\0';
    const std::string cmd = "objdump -d -C -M intel --no-show-raw-insn --no-addresses ";
    FILE* p = popen((cmd + exe).c_str(), "r");
    if (!p) return 1;
    char line[512];
    bool show = false;
    int shown = 0, hidden = 0;
    while (std::fgets(line, sizeof line, p)) {
        std::string s(line);
        if (s.front() == '<') {                       // "<name(args)>:" starts a function
            show = false;
            shown = 0;
            for (const char* f : {"<qty_at(", "<div10(", "<best(", "<fee_bps(", "<sum(",
                                  "<route(", "<record("})
                show = show || s.rfind(f, 0) == 0;
        }
        if (!show || s == "\n") continue;
        if (++shown > 24) { ++hidden; continue; }       // record() is long: cut it short
        if (s.size() > 80) s = s.substr(0, 76) + " ...\n";   // long C++ names
        std::fputs(s.c_str(), stdout);
    }
    std::printf("\t... %d more instructions (reallocation path) not shown\n", hidden);
    return pclose(p) == 0 ? 0 : 1;
}
