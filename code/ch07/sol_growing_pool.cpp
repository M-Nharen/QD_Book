// Solution P7.10: an object pool that grows in fixed-size chunks (objects never move),
// with a liveness flag per slot to catch double destroy and foreign pointers.
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>
#include <vector>

template <class T, std::size_t ChunkSlots = 1024> class GrowingPool {
    struct Slot {
        union {
            Slot* next;                            // valid while free
            alignas(T) std::byte storage[sizeof(T)];   // valid while live
        };
        bool live = false;
        Slot() : next(nullptr) {}
    };
    std::vector<std::unique_ptr<Slot[]>> chunks_;
    Slot* free_ = nullptr;

    void grow() {                                  // new chunk; old chunks stay put
        chunks_.push_back(std::make_unique<Slot[]>(ChunkSlots));
        Slot* c = chunks_.back().get();
        for (std::size_t i = ChunkSlots; i-- > 0;) {
            c[i].next = free_;
            free_ = &c[i];
        }
    }
    static Slot* slot_of(T* p) {                   // storage is the first member, so
        return reinterpret_cast<Slot*>(p);         // the addresses coincide
    }

public:
    GrowingPool() = default;
    GrowingPool(const GrowingPool&) = delete;
    GrowingPool& operator=(const GrowingPool&) = delete;
    ~GrowingPool() {                               // destroy anything still live
        for (auto& c : chunks_)
            for (std::size_t i = 0; i < ChunkSlots; ++i)
                if (c[i].live) std::launder(reinterpret_cast<T*>(c[i].storage))->~T();
    }

    template <class... A> T* create(A&&... args) {
        if (!free_) grow();                        // the only allocation, amortized
        Slot* s = free_;
        free_ = s->next;                           // read BEFORE constructing: T's bytes
        T* p;                                      // overwrite next (it is a union)
        try {
            p = ::new (static_cast<void*>(s->storage)) T(std::forward<A>(args)...);
        } catch (...) {                            // constructor threw: give slot back
            s->next = free_;
            free_ = s;
            throw;
        }
        s->live = true;
        return p;
    }
    void destroy(T* p) {
        Slot* s = slot_of(p);
        if (!s->live) {                            // double destroy: fail loudly, now
            std::fprintf(stderr, "GrowingPool: destroy of a non-live object\n");
            std::abort();
        }
        p->~T();
        s->live = false;
        s->next = free_;
        free_ = s;
    }
    std::size_t chunks() const { return chunks_.size(); }
};

struct Order { long id, price, qty; };

int main() {
    GrowingPool<Order> pool;
    std::vector<Order*> v;
    for (long i = 0; i < 10'000; ++i) v.push_back(pool.create(Order{i, 100, 1}));
    const Order* first = v.front();
    std::printf("10,000 creates -> %zu chunks of 1024; first object id %ld\n",
                pool.chunks(), first->id);
    for (Order* o : v) pool.destroy(o);
    v.clear();
    for (long i = 0; i < 10'000; ++i) v.push_back(pool.create(Order{i, 101, 2}));
    std::printf("after destroying all and creating 10,000 again: %zu chunks\n",
                pool.chunks());
    for (std::size_t i = 0; i < v.size(); i += 2) pool.destroy(v[i]);   // half stay live:
    std::printf("destructor cleans up the 5,000 still live\n");          // ~GrowingPool
}
