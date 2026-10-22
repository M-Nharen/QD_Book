// What input/output style costs: reading 2*10^6 integers, writing 10^6 lines (3.11).
// Each reader runs in a fresh child process (stream state cannot be reset in-process),
// with stdin redirected to a temporary file.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
using ll = long long;
using Clock = std::chrono::steady_clock;
constexpr int N = 2'000'000;

static char buf[1 << 16];
static std::size_t pos = 0, len = 0;
inline int gc() {
    if (pos == len) {
        len = std::fread(buf, 1, sizeof buf, stdin);
        pos = 0;
        if (len == 0) return -1;
    }
    return buf[pos++];
}
inline ll read_fast() {                            // same parser as Listing 3.1
    int c = gc();
    while (c != -1 && c != '-' && (c < '0' || c > '9')) c = gc();
    const bool neg = (c == '-');
    if (neg) c = gc();
    ll x = 0;
    for (; c >= '0' && c <= '9'; c = gc()) x = x * 10 + (c - '0');
    return neg ? -x : x;
}

int child(const char* mode) {                      // runs with stdin = the data file
    const auto t0 = Clock::now();
    ll sum = 0, x = 0;
    if (!std::strcmp(mode, "cin_nosync")) {
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);
    }
    if (!std::strcmp(mode, "scanf")) {
        for (int i = 0; i < N && std::scanf("%lld", &x) == 1; ++i) sum += x;
    } else if (!std::strcmp(mode, "fread")) {
        for (int i = 0; i < N; ++i) sum += read_fast();
    } else {
        for (int i = 0; i < N && (std::cin >> x); ++i) sum += x;
    }
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    std::printf("read  %-24s %8.1f ms  (%5.1f ns/int)  sum %lld\n", mode, ms,
                ms * 1e6 / N, sum);
    return 0;
}

template <class F>
void time_output(const char* name, F f) {
    std::ofstream out("/dev/null");                // every byte still goes through write()
    const auto t0 = Clock::now();
    f(out);
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    std::printf("write %-24s %8.1f ms\n", name, ms);
}

int main(int argc, char** argv) {
    if (argc > 1) return child(argv[1]);

    char path[] = "/tmp/qd_io_XXXXXX";             // write the input file once
    const int fd = mkstemp(path);
    if (fd < 0) { std::perror("mkstemp"); return 1; }
    {
        std::mt19937_64 rng(5);
        std::uniform_int_distribution<ll> d(-1'000'000'000, 1'000'000'000);
        std::string text;
        for (int i = 0; i < N; ++i) text += std::to_string(d(rng)) + (i % 10 ? ' ' : '\n');
        if (write(fd, text.data(), text.size()) != ssize_t(text.size())) return 1;
        close(fd);
        std::printf("input: %d integers, %zu bytes\n", N, text.size());
    }
    for (const char* mode : {"cin_sync", "cin_nosync", "scanf", "fread"}) {
        std::fflush(stdout);                       // else the child inherits unflushed text
        const pid_t pid = fork();
        if (pid == 0) {
            const int in = open(path, O_RDONLY);
            dup2(in, 0);                           // the child's stdin is the file
            execl("/proc/self/exe", argv[0], mode, static_cast<char*>(nullptr));
            _exit(127);                            // only reached if exec failed
        }
        int status = 0;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return 1;
    }
    unlink(path);

    const int L = 1'000'000;
    time_output("ofstream << endl", [&](std::ofstream& o) {
        for (int i = 0; i < L; ++i) o << i << std::endl;    // flush = one write() per line
    });
    time_output("ofstream << '\\n'", [&](std::ofstream& o) {
        for (int i = 0; i < L; ++i) o << i << '\n';         // flushed when buffer fills
    });
}
