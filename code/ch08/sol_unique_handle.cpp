// Solution P8.8: one move-only RAII owner for any handle type, parameterized by traits.
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

template <class Traits> class UniqueHandle {
public:
    using handle_type = typename Traits::handle_type;
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(handle_type h) noexcept : h_(h) {}
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& o) noexcept : h_(o.release()) {}
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        reset(o.release());                        // self-move: release() then reset()
        return *this;                              // of the same value is harmless
    }
    ~UniqueHandle() { reset(); }

    handle_type get() const noexcept { return h_; }
    explicit operator bool() const noexcept { return h_ != Traits::invalid(); }
    handle_type release() noexcept { return std::exchange(h_, Traits::invalid()); }
    void reset(handle_type h = Traits::invalid()) noexcept {
        const handle_type old = std::exchange(h_, h);
        if (old != Traits::invalid() && old != h) Traits::close(old);
    }

private:
    handle_type h_ = Traits::invalid();
};

struct FdTraits {
    using handle_type = int;
    static constexpr int invalid() noexcept { return -1; }
    static void close(int fd) noexcept { ::close(fd); }
};
using Fd = UniqueHandle<FdTraits>;

struct FileTraits {
    using handle_type = std::FILE*;
    static constexpr std::FILE* invalid() noexcept { return nullptr; }
    static void close(std::FILE* f) noexcept { std::fclose(f); }
};
using File = UniqueHandle<FileTraits>;

static_assert(sizeof(Fd) == sizeof(int) && sizeof(File) == sizeof(std::FILE*));

int main() {
    Fd a(::open("/dev/null", O_RDONLY | O_CLOEXEC));
    Fd b = std::move(a);                           // a is now empty
    std::printf("a valid: %d, b valid: %d\n", bool(a), bool(b));
    Fd& alias = b;                                 // (a direct b = std::move(b) is
    b = std::move(alias);                          // caught by -Wself-move)
    std::printf("after self-move, b still valid: %d (fd %d)\n", bool(b), b.get());
    File f(std::fopen("/dev/null", "r"));
    std::printf("FILE* handle valid: %d; both closed automatically at scope exit\n",
                bool(f));
}
