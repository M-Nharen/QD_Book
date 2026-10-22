// Controlling template bloat: keep the template thin, move the work out of it (9.7).
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

// ---- Technique 1: a type-erased argument list. Each log() call site instantiates only
// a tiny wrapper; the formatting loop exists once, in a non-template function.
struct Arg {
    enum class Tag : std::uint8_t { I64, U64, F64, Str } tag;
    union { std::int64_t i; std::uint64_t u; double f; const char* s; };
};
inline Arg make_arg(std::signed_integral auto v) {
    Arg a{}; a.tag = Arg::Tag::I64; a.i = v; return a;
}
inline Arg make_arg(std::unsigned_integral auto v) {
    Arg a{}; a.tag = Arg::Tag::U64; a.u = v; return a;
}
inline Arg make_arg(std::floating_point auto v) {
    Arg a{}; a.tag = Arg::Tag::F64; a.f = v; return a;
}
inline Arg make_arg(const char* v) {
    Arg a{}; a.tag = Arg::Tag::Str; a.s = v; return a;
}

[[gnu::noinline]] void vlog(const char* fmt, const Arg* args, std::size_t n) {
    std::size_t k = 0;                        // '{}' placeholders, filled in order
    for (const char* p = fmt; *p; ++p) {
        if (p[0] == '{' && p[1] == '}' && k < n) {
            const Arg& a = args[k++];
            switch (a.tag) {
            case Arg::Tag::I64: std::printf("%lld", static_cast<long long>(a.i)); break;
            case Arg::Tag::U64: std::printf("%llu", static_cast<unsigned long long>(a.u));
                                break;
            case Arg::Tag::F64: std::printf("%g", a.f); break;
            case Arg::Tag::Str: std::fputs(a.s, stdout); break;
            }
            ++p;
        } else {
            std::putchar(*p);
        }
    }
    std::putchar('\n');
}

template <class... Ts>
inline void log(const char* fmt, const Ts&... vs) {   // the only per-signature code:
    const Arg args[] = {make_arg(vs)..., Arg{}};      // fill a small array, then call
    vlog(fmt, args, sizeof...(Ts));                   // (trailing Arg{} avoids a
}                                                     //  zero-length array)

// ---- Technique 2: hoist the code that does not depend on T into a non-template base.
// The cold error path below is compiled once, not once per FixedVec<T, N>.
class FixedVecBase {
protected:
    [[noreturn, gnu::cold, gnu::noinline]] static void overflow(std::size_t cap) {
        std::fprintf(stderr, "FixedVec overflow (capacity %zu)\n", cap);
        std::abort();
    }
};

template <class T, std::size_t N>
class FixedVec : FixedVecBase {
    static_assert(std::is_trivially_copyable_v<T>, "kept simple for the example");
public:
    void push_back(const T& v) {
        if (n_ == N) [[unlikely]] overflow(N);
        data_[n_++] = v;
    }
    std::size_t size() const { return n_; }
    const T& operator[](std::size_t i) const { return data_[i]; }
private:
    T data_[N];
    std::size_t n_ = 0;
};

int main() {
    log("order {} {} qty={} px={}", std::uint64_t{42}, "BUY", 300, 101.25);
    log("heartbeat seq={}", 7u);
    FixedVec<int, 4> a;
    FixedVec<double, 8> b;
    for (int i = 0; i < 4; ++i) { a.push_back(i); b.push_back(i * 0.5); }
    log("a.size={} b.size={} b[3]={}", a.size(), b.size(), b[3]);
}
