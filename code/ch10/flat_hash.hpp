// FlatHashMap: open addressing with linear probing, power-of-two capacity, one metadata
// byte per slot, and backward-shift deletion (no tombstones). Section 10.5.
#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <class K, class V, class Hash = std::hash<K>, class Eq = std::equal_to<K>>
class FlatHashMap {
    static_assert(std::is_nothrow_move_constructible_v<K> &&
                  std::is_nothrow_move_constructible_v<V>,
                  "rehash and erase relocate elements; their moves must not throw");
    struct Slot { K key; V value; };
    static constexpr std::uint8_t kEmpty = 0;     // ctrl byte: 0 = empty,
    static constexpr std::size_t kMinCapacity = 16; // 0x80 | 7 hash bits = full

public:
    FlatHashMap() = default;
    explicit FlatHashMap(std::size_t n) { reserve(n); }
    FlatHashMap(const FlatHashMap&) = delete;              // copying is Problem P10.9
    FlatHashMap& operator=(const FlatHashMap&) = delete;
    FlatHashMap(FlatHashMap&& o) noexcept { swap(o); }
    FlatHashMap& operator=(FlatHashMap&& o) noexcept {
        FlatHashMap tmp(std::move(o));
        swap(tmp);
        return *this;
    }
    ~FlatHashMap() { destroy_all(); deallocate(); }

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return cap_; }
    bool empty() const noexcept { return size_ == 0; }

    V* find(const K& k) {
        if (size_ == 0) return nullptr;
        const auto [i, found] = probe(k, hash_of(k));
        return found ? &slots_[i].value : nullptr;
    }
    const V* find(const K& k) const { return const_cast<FlatHashMap*>(this)->find(k); }
    bool contains(const K& k) const { return find(k) != nullptr; }

    // Inserts {k, V(args...)} if k is absent. Returns the value and whether it inserted.
    template <class... Args>
    std::pair<V*, bool> try_emplace(const K& k, Args&&... args) {
        if (cap_ == 0) rehash(kMinCapacity);
        const std::uint64_t h = hash_of(k);
        auto [i, found] = probe(k, h);
        if (found) return {&slots_[i].value, false};
        if ((size_ + 1) * 4 > cap_ * 3) {                   // keep load factor <= 3/4
            rehash(cap_ * 2);
            i = find_empty(h);                              // k is known to be absent
        }
        ::new (static_cast<void*>(&slots_[i])) Slot{k, V(std::forward<Args>(args)...)};
        ctrl_[i] = tag_of(h);                               // only now is the slot full
        ++size_;
        return {&slots_[i].value, true};
    }
    V& operator[](const K& k) { return *try_emplace(k).first; }

    bool erase(const K& k) {
        if (size_ == 0) return false;
        auto [i, found] = probe(k, hash_of(k));
        if (!found) return false;
        std::destroy_at(&slots_[i]);
        // Backward shift: pull later members of the cluster into the hole, so every
        // element stays reachable from its home slot without passing an empty slot.
        for (std::size_t j = (i + 1) & mask_; ctrl_[j] != kEmpty; j = (j + 1) & mask_) {
            const std::size_t home = hash_of(slots_[j].key) & mask_;
            if (((j - home) & mask_) >= ((j - i) & mask_)) { // hole lies on j's probe path
                ::new (static_cast<void*>(&slots_[i])) Slot(std::move(slots_[j]));
                std::destroy_at(&slots_[j]);
                ctrl_[i] = ctrl_[j];
                i = j;                                       // the hole moves to j
            }
        }
        ctrl_[i] = kEmpty;
        --size_;
        return true;
    }

    void reserve(std::size_t n) {                // capacity for n elements, no rehash
        std::size_t need = std::bit_ceil((n * 4 + 2) / 3);
        if (need < kMinCapacity) need = kMinCapacity;
        if (need > cap_) rehash(need);
    }
    void clear() noexcept {
        destroy_all();
        for (std::size_t i = 0; i < cap_; ++i) ctrl_[i] = kEmpty;
        size_ = 0;
    }
    template <class F> void for_each(F&& f) {     // visits (key, value) in slot order
        for (std::size_t i = 0; i < cap_; ++i)
            if (ctrl_[i] != kEmpty) f(std::as_const(slots_[i].key), slots_[i].value);
    }
    // For analysis only (Problem P10.6): slots inspected by a lookup of k.
    std::size_t probe_length(const K& k) const {
        if (cap_ == 0) return 1;
        const std::uint64_t h = hash_of(k);
        std::size_t n = 1;
        for (std::size_t i = h & mask_; ctrl_[i] != kEmpty; i = (i + 1) & mask_, ++n)
            if (ctrl_[i] == tag_of(h) && eq_(slots_[i].key, k)) return n;
        return n;
    }
    void swap(FlatHashMap& o) noexcept {
        std::swap(ctrl_, o.ctrl_); std::swap(slots_, o.slots_);
        std::swap(cap_, o.cap_); std::swap(mask_, o.mask_); std::swap(size_, o.size_);
        std::swap(hash_, o.hash_); std::swap(eq_, o.eq_);
    }

private:
    // The user's hash is passed through a strong 64-bit finalizer (from SplitMix64), so
    // a weak hash such as libstdc++'s identity hash for integers still spreads over
    // both the low bits (slot index) and the high bits (7-bit tag).
    std::uint64_t hash_of(const K& k) const {
        std::uint64_t x = static_cast<std::uint64_t>(hash_(k));
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }
    static std::uint8_t tag_of(std::uint64_t h) noexcept {
        return static_cast<std::uint8_t>(0x80 | (h >> 57));
    }
    // Returns {slot, true} if k is present, else {first empty slot of the run, false}.
    std::pair<std::size_t, bool> probe(const K& k, std::uint64_t h) const {
        const std::uint8_t tag = tag_of(h);
        for (std::size_t i = h & mask_;; i = (i + 1) & mask_) {
            const std::uint8_t c = ctrl_[i];
            if (c == kEmpty) return {i, false};
            if (c == tag && eq_(slots_[i].key, k)) return {i, true};  // tag filters
        }                                                               // most compares
    }
    std::size_t find_empty(std::uint64_t h) const noexcept {
        std::size_t i = h & mask_;
        while (ctrl_[i] != kEmpty) i = (i + 1) & mask_;
        return i;
    }
    void rehash(std::size_t new_cap) {           // new_cap: a power of two
        FlatHashMap fresh;
        fresh.allocate(new_cap);
        for (std::size_t i = 0; i < cap_; ++i) {
            if (ctrl_[i] == kEmpty) continue;
            const std::uint64_t h = fresh.hash_of(slots_[i].key);
            const std::size_t j = fresh.find_empty(h);
            ::new (static_cast<void*>(&fresh.slots_[j])) Slot(std::move(slots_[i]));
            fresh.ctrl_[j] = tag_of(h);
            std::destroy_at(&slots_[i]);
            ctrl_[i] = kEmpty;
        }
        fresh.size_ = size_;
        size_ = 0;
        swap(fresh);                              // fresh now owns the old, empty arrays
    }
    void allocate(std::size_t cap) {
        auto ctrl = std::make_unique<std::uint8_t[]>(cap);   // zeroed: all slots empty
        slots_ = std::allocator<Slot>{}.allocate(cap);       // raw; if this throws,
        ctrl_ = ctrl.release();                              // ctrl is freed for us
        cap_ = cap;
        mask_ = cap - 1;
    }
    void deallocate() noexcept {
        if (slots_) std::allocator<Slot>{}.deallocate(slots_, cap_);
        delete[] ctrl_;
        slots_ = nullptr; ctrl_ = nullptr; cap_ = mask_ = 0;
    }
    void destroy_all() noexcept {
        if constexpr (!std::is_trivially_destructible_v<Slot>)
            for (std::size_t i = 0; i < cap_; ++i)
                if (ctrl_[i] != kEmpty) std::destroy_at(&slots_[i]);
    }

    std::uint8_t* ctrl_ = nullptr;   // cap_ bytes: one per slot, scanned first
    Slot* slots_ = nullptr;          // cap_ slots: constructed only where ctrl_ != 0
    std::size_t cap_ = 0, mask_ = 0, size_ = 0;
    [[no_unique_address]] Hash hash_{};
    [[no_unique_address]] Eq eq_{};
};
