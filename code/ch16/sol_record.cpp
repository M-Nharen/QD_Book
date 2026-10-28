// Solution to P16.8: keep the reallocation path out of the function that appends, so
// the fast path needs no callee-saved registers. Checks its own disassembly.
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

class Log {
public:
    void push(int x) {
        if (end_ == cap_) [[unlikely]] grow();        // rare: out of line, cold
        *end_++ = x;
    }
    std::size_t size() const { return std::size_t(end_ - begin_); }
    ~Log() { std::free(begin_); }

private:
    [[gnu::noinline, gnu::cold]] void grow() {
        const std::size_t n = size(), cap = n ? 2 * n : 64;
        int* p = static_cast<int*>(std::realloc(begin_, cap * sizeof(int)));
        if (!p) std::abort();
        begin_ = p;
        end_ = p + n;
        cap_ = p + cap;
    }
    int* begin_ = nullptr;
    int* end_ = nullptr;
    int* cap_ = nullptr;
};

[[gnu::noinline]] void record_vec(std::vector<int>& log, int x) { log.push_back(x); }
[[gnu::noinline]] void record_log(Log& log, int x) { log.push(x); }

int count_pushes(const char* exe, const char* fn) {   // "push" instructions in fn
    const std::string cmd = std::string("objdump -d -C --no-show-raw-insn ") + exe;
    FILE* p = popen(cmd.c_str(), "r");
    char line[512];
    int pushes = 0;
    bool in = false;
    while (p && std::fgets(line, sizeof line, p)) {
        if (std::strchr(line, '<') && std::strstr(line, ">:"))
            in = std::strstr(line, fn) != nullptr;
        else if (in && std::strstr(line, "\tpush ")) ++pushes;
    }
    if (p) pclose(p);
    return pushes;
}

int main() {
    std::vector<int> v;
    Log log;
    for (int i = 0; i < 1000; ++i) { record_vec(v, i); record_log(log, i); }
    char exe[4096];
    const ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return 1;
    exe[n] = '\0';
    std::printf("record_vec: %d pushes, record_log: %d pushes (sizes %zu, %zu)\n",
                count_pushes(exe, "<record_vec("), count_pushes(exe, "<record_log("),
                v.size(), log.size());
}
