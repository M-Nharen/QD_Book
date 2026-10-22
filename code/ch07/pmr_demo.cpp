// std::pmr: standard containers on top of your own memory (Section 7.5).
#include <array>
#include <cstddef>
#include <cstdio>
#include <list>
#include <memory_resource>
#include <new>
#include <string>
#include <vector>

// A pass-through resource that counts what it is asked for.
class Counting : public std::pmr::memory_resource {
    std::pmr::memory_resource* up_;
public:
    std::size_t calls = 0, bytes = 0;
    explicit Counting(std::pmr::memory_resource* up) : up_(up) {}
private:
    void* do_allocate(std::size_t n, std::size_t al) override {
        ++calls;
        bytes += n;
        return up_->allocate(n, al);
    }
    void do_deallocate(void* p, std::size_t n, std::size_t al) override {
        up_->deallocate(p, n, al);
    }
    bool do_is_equal(const memory_resource& o) const noexcept override {
        return this == &o;
    }
};

int main() {
    std::printf("sizeof(std::vector<int>)=%zu  sizeof(std::pmr::vector<int>)=%zu\n",
                sizeof(std::vector<int>), sizeof(std::pmr::vector<int>));

    // 1. A fixed buffer with NO fallback: exceeding it throws instead of calling malloc.
    alignas(std::max_align_t) std::array<std::byte, 4096> buf;
    std::pmr::monotonic_buffer_resource mono(buf.data(), buf.size(),
                                             std::pmr::null_memory_resource());
    Counting count_mono(&mono);
    {
        std::pmr::vector<int> grow(&count_mono);
        for (int i = 0; i < 100; ++i) grow.push_back(i);   // no reserve()
        std::printf("100 push_backs, no reserve: %zu allocations, %zu bytes taken\n",
                    count_mono.calls, count_mono.bytes);
        std::printf("  (monotonic: the %zu bytes of discarded buffers are not reused)\n",
                    count_mono.bytes - grow.capacity() * sizeof(int));
    }
    try {
        std::pmr::vector<std::pmr::string> names(&mono);
        for (int i = 0; i < 1000; ++i)                     // strings get the vector's
            names.emplace_back("a string too long for SSO"); // allocator too
    } catch (const std::bad_alloc&) {
        std::printf("buffer exhausted -> std::bad_alloc (no silent fallback to malloc)\n");
    }

    // 2. A pool resource: many small nodes, few upstream allocations.
    Counting upstream(std::pmr::new_delete_resource());
    {
        std::pmr::unsynchronized_pool_resource pool(&upstream);
        std::pmr::list<long> nodes(&pool);
        for (int round = 0; round < 10; ++round) {
            for (int i = 0; i < 1000; ++i) nodes.push_back(i);
            nodes.clear();                                 // nodes return to the pool
        }
        std::printf("10,000 list-node allocations through a pool: %zu upstream calls\n",
                    upstream.calls);
    }
}
