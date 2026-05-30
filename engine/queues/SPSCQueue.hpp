#pragma once

#include <atomic>
#include <array>
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace trading {

// Single-producer single-consumer lock-free ring buffer.
// Capacity must be a power of two. T must be trivially copyable.
//
// Memory ordering: acquire/release only — seq_cst is forbidden.
//   producer: tail_.load(relaxed), head_.load(acquire), tail_.store(release)
//   consumer: head_.load(relaxed), tail_.load(acquire), head_.store(release)
//
// Cache padding: head_ and tail_ are on separate cache lines to prevent
// false sharing between the producer and consumer threads.
template<typename T, std::size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");
    static_assert(Capacity >= 2,
                  "Capacity must be at least 2");

    static constexpr std::size_t MASK       = Capacity - 1;
    static constexpr std::size_t kCacheLine = 64;

    alignas(kCacheLine) std::atomic<uint64_t> head_{0};  // written by consumer
    alignas(kCacheLine) std::atomic<uint64_t> tail_{0};  // written by producer
    alignas(kCacheLine) std::array<T, Capacity> buffer_{};

public:
    SPSCQueue() = default;

    SPSCQueue(const SPSCQueue&)            = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Called by producer thread only.
    // Returns true if the item was enqueued, false if the queue is full.
    bool enqueue(const T& item) noexcept {
        const uint64_t tail = tail_.load(std::memory_order_relaxed);
        const uint64_t head = head_.load(std::memory_order_acquire);

        if (tail - head >= Capacity) {
            return false;
        }

        buffer_[tail & MASK] = item;
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Called by consumer thread only.
    // Returns true if an item was dequeued into out, false if the queue is empty.
    bool dequeue(T& out) noexcept {
        const uint64_t head = head_.load(std::memory_order_relaxed);
        const uint64_t tail = tail_.load(std::memory_order_acquire);

        if (head >= tail) {
            return false;
        }

        out = buffer_[head & MASK];
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    // Approximate — safe to call from either thread for monitoring only.
    std::size_t size_approx() const noexcept {
        const uint64_t tail = tail_.load(std::memory_order_acquire);
        const uint64_t head = head_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(tail - head);
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) >=
               tail_.load(std::memory_order_acquire);
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }
};

} // namespace trading
