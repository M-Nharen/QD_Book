// Move semantics with a tracing type: std::move is only a cast; what moves is the
// constructor or assignment that overload resolution picks (Section 8.2).
#include <cstdio>
#include <string>
#include <utility>

struct Counts { int copy_ctor = 0, move_ctor = 0, copy_assign = 0, move_assign = 0; };
Counts g;

struct Tracer {                                    // owns heap memory, like most real types
    std::string payload;
    Tracer() = default;
    explicit Tracer(std::string p) : payload(std::move(p)) {}
    Tracer(const Tracer& o) : payload(o.payload) { ++g.copy_ctor; }
    Tracer(Tracer&& o) noexcept : payload(std::move(o.payload)) { ++g.move_ctor; }
    Tracer& operator=(const Tracer& o) {
        payload = o.payload;
        ++g.copy_assign;
        return *this;
    }
    Tracer& operator=(Tracer&& o) noexcept {
        payload = std::move(o.payload);
        ++g.move_assign;
        return *this;
    }
};

// Runs f, prints the counts; f may return a number worth showing (or -1 for none).
template <class F> void report(const char* what, F f) {
    g = {};
    const long extra = f();
    std::printf("  %-42s copy %d+%d  move %d+%d", what, g.copy_ctor, g.copy_assign,
                g.move_ctor, g.move_assign);
    if (extra >= 0) std::printf("  [a.payload.size() = %ld]", extra);
    std::printf("\n");
}

struct Order {
    Tracer tag;
    void set_by_cref(const Tracer& t) { tag = t; }            // always copies
    void set_by_value(Tracer t) { tag = std::move(t); }       // "sink" parameter
    void set_overloaded(const Tracer& t) { tag = t; }         // the optimal pair:
    void set_overloaded(Tracer&& t) { tag = std::move(t); }   // two overloads
};

const std::string kLong(40, 'x');                  // too long for SSO: copies allocate

int main() {
    std::printf("(columns: copy ctor+assign, move ctor+assign)\n");
    report("Tracer b = a", [] { Tracer a{kLong}; Tracer b = a; return -1L; });
    report("Tracer b = std::move(a)", [] {
        Tracer a{kLong};
        Tracer b = std::move(a);
        return long(a.payload.size());             // moved-from: valid, unspecified
    });
    report("Tracer&& r = std::move(a)", [] {
        Tracer a{kLong};
        Tracer&& r = std::move(a);                 // a cast, then a reference binding:
        return long(r.payload.size());             // nothing has moved
    });
    report("const a; Tracer b = std::move(a)", [] {
        const Tracer a{kLong};
        Tracer b = std::move(a);                   // const Tracer&& binds to const& only
        return -1L;
    });

    Tracer lv{kLong};
    Order o;
    std::printf("Setting a member from an lvalue / from an rvalue:\n");
    report("set_by_cref(lvalue)", [&] { o.set_by_cref(lv); return -1L; });
    report("set_by_cref(rvalue)", [&] { o.set_by_cref(Tracer{kLong}); return -1L; });
    report("set_by_value(lvalue)", [&] { o.set_by_value(lv); return -1L; });
    report("set_by_value(rvalue)", [&] { o.set_by_value(Tracer{kLong}); return -1L; });
    report("set_overloaded(lvalue)", [&] { o.set_overloaded(lv); return -1L; });
    report("set_overloaded(rvalue)", [&] { o.set_overloaded(Tracer{kLong}); return -1L; });
}
