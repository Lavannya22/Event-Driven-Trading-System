#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Open-addressing flat hash map for order lookup.
// Storage is pre-allocated at construction — no runtime malloc.
// Capacity must be a power of two; load factor is kept below 50%.
// Order ID 0 is reserved as the empty sentinel and must not be inserted.

namespace trading {

struct OrderEntry;  // forward-declared; defined in OrderBook.hpp

template<typename V>
class FlatOrderMap {
    struct Slot {
        uint64_t key;  // 0 = empty, UINT64_MAX = tombstone
        V        value;
    };

    static constexpr uint64_t EMPTY_KEY     = 0;
    static constexpr uint64_t TOMBSTONE_KEY = UINT64_MAX;

    std::vector<Slot> table_;  // allocated once in constructor
    std::size_t       count_{0};
    std::size_t       cap_;

    std::size_t probe(uint64_t key, std::size_t start) const noexcept {
        return (start + 1) & (cap_ - 1);
    }

public:
    explicit FlatOrderMap(std::size_t capacity) : cap_(capacity) {
        table_.resize(capacity, {EMPTY_KEY, {}});  // startup allocation
    }

    bool insert(uint64_t key, const V& value) noexcept {
        if (key == EMPTY_KEY || key == TOMBSTONE_KEY) return false;
        if (count_ * 2 >= cap_) return false;  // 50% load factor

        std::size_t idx = key & (cap_ - 1);
        for (std::size_t i = 0; i < cap_; ++i, idx = probe(key, idx)) {
            Slot& s = table_[idx];
            if (s.key == EMPTY_KEY || s.key == TOMBSTONE_KEY) {
                s = {key, value};
                ++count_;
                return true;
            }
            if (s.key == key) return false;  // duplicate
        }
        return false;
    }

    V* find(uint64_t key) noexcept {
        if (key == EMPTY_KEY || key == TOMBSTONE_KEY) return nullptr;
        std::size_t idx = key & (cap_ - 1);
        for (std::size_t i = 0; i < cap_; ++i, idx = probe(key, idx)) {
            Slot& s = table_[idx];
            if (s.key == EMPTY_KEY) return nullptr;
            if (s.key == key) return &s.value;
        }
        return nullptr;
    }

    const V* find(uint64_t key) const noexcept {
        if (key == EMPTY_KEY || key == TOMBSTONE_KEY) return nullptr;
        std::size_t idx = key & (cap_ - 1);
        for (std::size_t i = 0; i < cap_; ++i, idx = probe(key, idx)) {
            const Slot& s = table_[idx];
            if (s.key == EMPTY_KEY) return nullptr;
            if (s.key == key) return &s.value;
        }
        return nullptr;
    }

    bool erase(uint64_t key) noexcept {
        if (key == EMPTY_KEY || key == TOMBSTONE_KEY) return false;
        std::size_t idx = key & (cap_ - 1);
        for (std::size_t i = 0; i < cap_; ++i, idx = probe(key, idx)) {
            Slot& s = table_[idx];
            if (s.key == EMPTY_KEY) return false;
            if (s.key == key) {
                s.key = TOMBSTONE_KEY;
                --count_;
                return true;
            }
        }
        return false;
    }

    std::size_t size()     const noexcept { return count_; }
    std::size_t capacity() const noexcept { return cap_; }
};

} // namespace trading
