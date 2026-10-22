#include <cstdio>
long fee_from_a(long);
long fee_from_b(long);
int main() {
    std::printf("a: %ld  b: %ld\n", fee_from_a(1'000'000), fee_from_b(1'000'000));
}
