// A class that owns a raw resource needs all five special members (Section 5.5).
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>

class Buffer {
    std::size_t size_ = 0;
    char* data_ = nullptr;                       // owning pointer: we must free it
public:
    Buffer() = default;
    explicit Buffer(std::size_t n) : size_(n), data_(new char[n]) {
        std::memset(data_, 0, n);
    }
    ~Buffer() { delete[] data_; }                                  // 1. destructor

    Buffer(const Buffer& o) : size_(o.size_), data_(nullptr) {     // 2. copy ctor
        if (size_) {
            data_ = new char[size_];             // deep copy: our own allocation
            std::memcpy(data_, o.data_, size_);
        }
    }
    Buffer(Buffer&& o) noexcept                                    // 3. move ctor
        : size_(std::exchange(o.size_, 0)), data_(std::exchange(o.data_, nullptr)) {}

    Buffer& operator=(Buffer o) noexcept {       // 4+5. copy AND move assignment:
        swap(o);                                 // o was copied or moved in; we take
        return *this;                            // its state, it takes ours and frees
    }                                            // it when it dies (copy-and-swap)
    void swap(Buffer& o) noexcept {
        std::swap(size_, o.size_);
        std::swap(data_, o.data_);
    }
    std::size_t size() const { return size_; }
    char* data() { return data_; }
};

int main() {
    Buffer a(16);
    std::strcpy(a.data(), "ORDER-1");
    Buffer b = a;                                // copy: independent bytes
    b.data()[6] = '2';
    Buffer c = std::move(a);                     // move: steals, a is left empty
    Buffer& alias = c;
    c = alias;                                   // self-assignment is safe
    b = std::move(c);                            // move-assign
    std::printf("a.size()=%zu  b=%s  c.size()=%zu\n", a.size(), b.data(), c.size());
}
