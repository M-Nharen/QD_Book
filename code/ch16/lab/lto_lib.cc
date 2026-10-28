// Lab kernel for Section 16.1: a tiny function in its own translation unit.
#include <cstdint>

std::int64_t notional(std::int32_t px, std::int32_t qty) { return std::int64_t{px} * qty; }
