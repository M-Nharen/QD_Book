// Where base-class subobjects live, and the pointer adjustments that follow (6.3).
#include <cstdint>
#include <cstdio>

struct Listener {                                // interface 1
    virtual ~Listener() = default;
    virtual void on_fill(long qty) = 0;
};
struct Logger {                                  // interface 2
    virtual ~Logger() = default;
    virtual void log(long v) = 0;
};
struct Strategy : Listener, Logger {             // multiple inheritance
    long pos = 0, last = 0;
    void on_fill(long qty) override { pos += qty; }
    void log(long v) override { last = v; }
};

struct Instrument { long id = 0; };              // diamond, without and with virtual
struct EquityA : Instrument { long lot = 0; };
struct ListedA : Instrument { long venue = 0; };
struct StockA : EquityA, ListedA {};             // two Instrument subobjects
struct EquityV : virtual Instrument { long lot = 0; };
struct ListedV : virtual Instrument { long venue = 0; };
struct StockV : EquityV, ListedV {};             // one shared Instrument subobject

long offset(const void* whole, const void* part) {
    return static_cast<long>(reinterpret_cast<std::uintptr_t>(part) -
                             reinterpret_cast<std::uintptr_t>(whole));
}

int main() {
    Strategy s;
    Listener* as_listener = &s;                  // first base: offset 0
    Logger* as_logger = &s;                      // second base: pointer is adjusted
    std::printf("sizeof(Strategy) = %zu\n", sizeof(Strategy));
    std::printf("Listener subobject at +%ld, Logger subobject at +%ld\n",
                offset(&s, as_listener), offset(&s, as_logger));
    std::printf("&s == as_logger compares equal: %s (the compiler adjusts first)\n",
                &s == as_logger ? "yes" : "no");
    Logger* wrong = reinterpret_cast<Logger*>(&s);   // no adjustment: points at Listener
    std::printf("reinterpret_cast<Logger*>(&s) is off by %ld bytes\n",
                offset(wrong, as_logger));
    Strategy* none = nullptr;
    Logger* still_none = none;                   // null must stay null: a branch
    std::printf("null Strategy* -> Logger*: %s\n", still_none ? "non-null!" : "null");
    as_logger->log(42);                          // dispatched through a thunk
    std::printf("s.last = %ld\n", s.last);

    std::printf("non-virtual diamond: sizeof(StockA) = %zu (two ids)\n", sizeof(StockA));
    std::printf("virtual diamond:     sizeof(StockV) = %zu (one id, two vptrs)\n",
                sizeof(StockV));
    StockV v;
    v.id = 7;                                    // unambiguous: one shared Instrument
    EquityV& e = v;
    ListedV& l = v;
    std::printf("Instrument via EquityV at +%ld, via ListedV at +%ld\n",
                offset(&v, static_cast<Instrument*>(&e)),
                offset(&v, static_cast<Instrument*>(&l)));
}
