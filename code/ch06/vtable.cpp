// The vptr made visible: per-type tables, construction order, RTTI (6.4).
#include <cstdint>
#include <cstdio>
#include <typeinfo>

std::uintptr_t vptr_of(const void* obj) {        // first 8 bytes of a polymorphic object
    const unsigned char* p = static_cast<const unsigned char*>(obj);
    std::uintptr_t v = 0;
    for (int i = 7; i >= 0; --i) v = (v << 8) | p[i];   // little-endian load, byte-wise
    return v;
}

bool tracing = false;                            // record only while building b1
std::uintptr_t seen_in_base_ctor = 0, seen_in_derived_ctor = 0;

struct Plain { long id; };                       // no virtual functions: no vptr

struct Handler {
    long id = 0;
    Handler() {
        if (!tracing) return;
        seen_in_base_ctor = vptr_of(this);
        std::printf("  Handler() calls name(): %s\n", name());
    }
    virtual ~Handler() = default;
    virtual const char* name() const { return "Handler"; }
};

struct BookHandler : Handler {
    BookHandler() {
        if (!tracing) return;
        seen_in_derived_ctor = vptr_of(this);
        std::printf("  BookHandler() calls name(): %s\n", name());
    }
    const char* name() const override { return "BookHandler"; }
};

struct TradeHandler : Handler {
    const char* name() const override { return "TradeHandler"; }
};

const char* same(std::uintptr_t a, std::uintptr_t b) {
    return a == b ? "same" : "different";
}

int main() {
    std::printf("sizeof(Plain) = %zu, sizeof(Handler) = %zu\n", sizeof(Plain),
                sizeof(Handler));
    std::printf("constructing a BookHandler:\n");
    tracing = true;
    BookHandler b1;
    tracing = false;
    BookHandler b2;
    TradeHandler t;
    Handler h;
    const std::uintptr_t vb = vptr_of(&b1), vt = vptr_of(&t), vh = vptr_of(&h);
    std::printf("vptr of b1 vs b2: %s table\n", same(vb, vptr_of(&b2)));
    std::printf("vptr of b1 vs t:  %s table\n", same(vb, vt));
    std::printf("vptr during Handler() vs a plain Handler:  %s\n",
                same(seen_in_base_ctor, vh));
    std::printf("vptr during BookHandler() vs finished b1:  %s\n",
                same(seen_in_derived_ctor, vb));

    Handler* p = &b1;
    std::printf("p->name() = %s, typeid(*p).name() = %s\n", p->name(), typeid(*p).name());
    std::printf("dynamic_cast<BookHandler*>(p):  %s\n",
                dynamic_cast<BookHandler*>(p) ? "ok" : "nullptr");
    std::printf("dynamic_cast<TradeHandler*>(p): %s\n",
                dynamic_cast<TradeHandler*>(p) ? "ok" : "nullptr");
}
