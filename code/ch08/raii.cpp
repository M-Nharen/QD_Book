// RAII: a move-only file-descriptor owner, a scope guard, and destruction order during
// normal exit and during exception unwinding (Section 8.5).
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>
#include <utility>

class UniqueFd {                                   // owns exactly one file descriptor
    int fd_ = -1;
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    UniqueFd(const UniqueFd&) = delete;            // two owners would close twice
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    UniqueFd& operator=(UniqueFd&& o) noexcept {
        if (this != &o) reset(std::exchange(o.fd_, -1));
        return *this;
    }
    ~UniqueFd() { reset(); }
    void reset(int fd = -1) noexcept {
        if (fd_ >= 0) ::close(fd_);
        fd_ = fd;
    }
    int get() const noexcept { return fd_; }
    explicit operator bool() const noexcept { return fd_ >= 0; }
};

template <class F> class ScopeExit {               // run f at scope exit unless dismissed
    F f_;
    bool active_ = true;
public:
    explicit ScopeExit(F f) noexcept : f_(std::move(f)) {}
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ~ScopeExit() { if (active_) f_(); }
    void dismiss() noexcept { active_ = false; }
};

struct Noisy {
    const char* name;
    explicit Noisy(const char* n) : name(n) { std::printf("    construct %s\n", name); }
    ~Noisy() { std::printf("    destroy   %s\n", name); }
};

long open_fds() {
    long n = 0;
    for ([[maybe_unused]] auto& e : std::filesystem::directory_iterator("/proc/self/fd"))
        ++n;
    return n;
}

UniqueFd open_null() { return UniqueFd(::open("/dev/null", O_RDONLY | O_CLOEXEC)); }

void risky(bool fail) {
    Noisy a("a");
    UniqueFd f = open_null();
    Noisy b("b");
    if (fail) throw std::runtime_error("risk check failed");
    std::printf("    risky finished normally\n");
}                                                  // b, f, a destroyed: reverse order

int main() {
    const long base = open_fds();
    std::printf("Normal exit:\n");
    risky(false);
    std::printf("Exception thrown inside:\n");
    try {
        risky(true);
    } catch (const std::exception& e) {
        std::printf("    caught: %s\n", e.what());
    }
    std::printf("descriptors leaked: %ld\n", open_fds() - base);

    UniqueFd owner = open_null();
    UniqueFd other = std::move(owner);             // ownership moves; no close, no dup
    std::printf("after move: owner %s, other holds fd %d\n",
                owner ? "still owns" : "is empty", other.get());

    std::printf("Scope guards:\n");
    {
        ScopeExit undo([] { std::printf("    rollback: cancel the half-sent order\n"); });
        std::printf("    step 1 done, step 2 fails -> leave without dismissing\n");
    }
    {
        ScopeExit undo([] { std::printf("    (never printed)\n"); });
        std::printf("    both steps done -> dismiss\n");
        undo.dismiss();
    }
}
