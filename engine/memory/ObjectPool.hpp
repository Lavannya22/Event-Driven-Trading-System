#pragma once

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace trading {

// Fixed-capacity pool of T objects.
//
// All storage is allocated in the constructor (startup time).
// allocate() and release() perform no heap operations — O(1) stack push/pop.
//
// On exhaustion: allocate() returns nullptr and increments exhaustion_count().
// Callers must handle nullptr — the pool never blocks, throws, or calls malloc.
//
// T must be trivially destructible (pool does not invoke destructors).
template<typename T>
class ObjectPool {
    static_assert(std::is_trivially_destructible_v<T>,
                  "ObjectPool<T>: T must be trivially destructible");

public:
    explicit ObjectPool(std::size_t capacity)
        : storage_(capacity), free_list_(capacity)
    {
        for (std::size_t i = 0; i < capacity; ++i)
            free_list_[i] = &storage_[i];
        top_ = capacity;
    }

    ObjectPool(const ObjectPool&)            = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Returns a pointer to a free slot, or nullptr if the pool is exhausted.
    T* allocate() noexcept {
        if (top_ == 0) {
            ++exhaustion_count_;
            return nullptr;
        }
        return free_list_[--top_];
    }

    // Returns a previously allocated slot back to the pool.
    // obj must be a pointer originally returned by allocate() on this pool.
    void release(T* obj) noexcept {
        assert(obj != nullptr);
        assert(top_ < free_list_.size());
        free_list_[top_++] = obj;
    }

    std::size_t capacity()         const noexcept { return storage_.size(); }
    std::size_t available()        const noexcept { return top_; }
    std::size_t used()             const noexcept { return storage_.size() - top_; }
    std::size_t exhaustion_count() const noexcept { return exhaustion_count_; }

private:
    std::vector<T>    storage_;     // pre-allocated object storage
    std::vector<T*>   free_list_;   // stack of pointers to free slots
    std::size_t       top_{0};
    std::size_t       exhaustion_count_{0};
};

} // namespace trading
