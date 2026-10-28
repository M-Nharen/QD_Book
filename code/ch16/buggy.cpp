// book: flags=-pthread
// Four classic bugs for practising with the sanitizers and gdb (Section 16.6). Run with
// no argument, it does nothing wrong, so the book's builds pass; each argument triggers
// one bug: uaf (use after free), overflow (signed overflow), race (data race),
// corrupt (an off-by-one write into the next field, which no sanitizer reports).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

struct Position {
    char symbol[8];
    std::int32_t qty;                                  // the victim of "corrupt"
};

std::string_view venue_name(int id) {
    const std::string name = "XNAS-" + std::to_string(id) + "-primary-listing";
    return name;                                       // view of a destroyed string
}

std::int32_t notional(std::int32_t px, std::int32_t qty) { return px * qty; }

int main(int argc, char** argv) {
    const std::string bug = argc > 1 ? argv[1] : "";
    if (bug == "uaf") {
        std::printf("%zu\n", venue_name(7).size() + std::string(venue_name(7)).size());
    } else if (bug == "overflow") {
        std::printf("%d\n", notional(argc * 50'000, 50'000));   // 2.5e9 > INT32_MAX
    } else if (bug == "race") {
        long fills = 0;
        std::thread t([&] { for (int i = 0; i < 100000; ++i) ++fills; });
        for (int i = 0; i < 100000; ++i) ++fills;
        t.join();
        std::printf("%ld\n", fills);
    } else if (bug == "corrupt") {
        Position p{};
        p.qty = 100;
        const std::string sym = "AAPL.OQX";            // 8 characters: fills symbol[8]
        std::memcpy(p.symbol, sym.c_str(), sym.size() + 1);   // +1 copies the '\0' too
        std::printf("qty = %d\n", p.qty);
    } else {
        std::printf("no bug selected\n");
    }
}
