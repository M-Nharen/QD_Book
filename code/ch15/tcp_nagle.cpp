// book: flags=-pthread
// Nagle's algorithm meets delayed ACKs (Section 15.1). A client sends each request as two
// small write()s (header, then body) and waits for a 1-byte reply; the server replies only
// after reading the whole 40-byte request. Measured over TCP on loopback. tsan-clean.
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

static bool read_full(int fd, char* p, int n) {      // TCP is a byte stream: loop
    while (n > 0) {
        const ssize_t r = read(fd, p, n);
        if (r <= 0) return false;
        p += r;
        n -= static_cast<int>(r);
    }
    return true;
}

enum class Mode { TwoWrites, TwoWritesNoDelay, OneWritev };

void run(Mode mode, int iters, const char* label) {
    const int ls = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);       // port 0: kernel picks one
    socklen_t alen = sizeof a;
    if (bind(ls, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0 || listen(ls, 1) != 0 ||
        getsockname(ls, reinterpret_cast<sockaddr*>(&a), &alen) != 0) {
        perror("listen");
        return;
    }

    std::thread server([ls, iters] {
        const int c = accept(ls, nullptr, nullptr);
        char req[40];
        for (int i = 0; i < iters && read_full(c, req, sizeof req); ++i)
            if (write(c, "k", 1) != 1) break;          // reply after the WHOLE request
        close(c);
    });

    const int c = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(c, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) { perror("connect"); }
    const int one = 1;
    if (mode == Mode::TwoWritesNoDelay)
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    char header[8]{}, body[32]{}, reply;
    std::vector<double> us;
    for (int i = 0; i < iters; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        bool ok;
        if (mode == Mode::OneWritev) {                 // one syscall, one segment
            iovec v[2] = {{header, sizeof header}, {body, sizeof body}};
            ok = writev(c, v, 2) == 40;
        } else {
            ok = write(c, header, sizeof header) == 8 && write(c, body, sizeof body) == 32;
        }
        if (!ok || !read_full(c, &reply, 1)) break;
        us.push_back(std::chrono::duration<double, std::micro>(
                         std::chrono::steady_clock::now() - t0).count());
    }
    close(c);
    server.join();
    close(ls);
    std::sort(us.begin(), us.end());
    std::printf("%-30s %5zu round trips: median %9.1f us, max %9.1f us\n", label, us.size(),
                us[us.size() / 2], us.back());
}

int main() {
    run(Mode::TwoWrites, 20, "two writes, Nagle on (default)");
    run(Mode::TwoWritesNoDelay, 2000, "two writes, TCP_NODELAY");
    run(Mode::OneWritev, 2000, "one writev, Nagle on");
}
