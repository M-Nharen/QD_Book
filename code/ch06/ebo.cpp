// Empty classes cost space as members, nothing as bases or [[no_unique_address]] (6.2).
#include <cstdint>
#include <cstdio>
#include <memory>

struct NoLock {                                  // a policy with no data
    void lock() {}
    void unlock() {}
};

struct CounterMember {                           // the empty policy as an ordinary member
    NoLock lock;
    std::int64_t value;
};
struct CounterBase : NoLock {                    // empty base optimization (EBO)
    std::int64_t value;
};
struct CounterAttr {                             // C++20: the member may overlap others
    [[no_unique_address]] NoLock lock;
    std::int64_t value;
};
struct TwoSame {                                 // two empty members of the SAME type
    [[no_unique_address]] NoLock a;              // must have distinct addresses,
    [[no_unique_address]] NoLock b;              // so at most one can overlap
    std::int32_t value;
};

void free_int(int* p) { delete p; }

int main() {
    std::printf("sizeof(NoLock)        = %zu  (every object needs a unique address)\n",
                sizeof(NoLock));
    std::printf("sizeof(CounterMember) = %zu  (1 byte + 7 bytes of padding)\n",
                sizeof(CounterMember));
    std::printf("sizeof(CounterBase)   = %zu\n", sizeof(CounterBase));
    std::printf("sizeof(CounterAttr)   = %zu\n", sizeof(CounterAttr));
    TwoSame t{};
    std::printf("sizeof(TwoSame)       = %zu, &a == &b: %s\n", sizeof(TwoSame),
                static_cast<void*>(&t.a) == static_cast<void*>(&t.b) ? "yes" : "no");

    auto lambda_deleter = [](int* p) { delete p; };              // stateless: empty
    std::unique_ptr<int> plain(new int(1));
    std::unique_ptr<int, decltype(lambda_deleter)> with_lambda(new int(2), lambda_deleter);
    std::unique_ptr<int, void (*)(int*)> with_fnptr(new int(3), &free_int);
    std::printf("unique_ptr: default deleter %zu, lambda deleter %zu, "
                "function-pointer deleter %zu bytes\n",
                sizeof(plain), sizeof(with_lambda), sizeof(with_fnptr));
}
