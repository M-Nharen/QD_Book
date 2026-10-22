// symbols.cc -- one translation unit, compiled with: g++ -O0 -std=c++20 -c symbols.cc
#include <cstdint>
std::int64_t order_count = 0;                 // external linkage, zero-initialized
std::int64_t tick_size = 5;                   // external linkage, constant-initialized
static int retries = 3;                       // internal linkage
namespace { int helper(int x) { return x * retries; } }   // internal linkage
void log_order(std::int64_t id);              // declared only: defined elsewhere
inline int twice(int x) { return 2 * x; }     // inline: may be defined in many TUs
int submit(std::int64_t id) {                 // external linkage, defined here
    ++order_count;
    log_order(id);
    return helper(twice(1));
}
