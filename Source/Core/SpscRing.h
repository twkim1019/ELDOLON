#pragma once
// EIDOLON — bounded, wait-free single-producer / single-consumer ring buffer.
// Producer: audio thread (never blocks, never allocates). Consumer: the message-thread engine.
#include <array>
#include <atomic>
#include <cstddef>

namespace eidolon
{
template <typename T, size_t Capacity>
class SpscRing
{
    static_assert ((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    /** Returns false (and drops the item) when full. Wait-free. */
    bool push (const T& item) noexcept
    {
        const size_t h = head.load (std::memory_order_relaxed);
        const size_t t = tail.load (std::memory_order_acquire);
        if (h - t >= Capacity) return false;
        buffer[h & (Capacity - 1)] = item;
        head.store (h + 1, std::memory_order_release);
        return true;
    }

    bool pop (T& out) noexcept
    {
        const size_t t = tail.load (std::memory_order_relaxed);
        const size_t h = head.load (std::memory_order_acquire);
        if (t == h) return false;
        out = buffer[t & (Capacity - 1)];
        tail.store (t + 1, std::memory_order_release);
        return true;
    }

    size_t size() const noexcept
    {
        return head.load (std::memory_order_acquire) - tail.load (std::memory_order_acquire);
    }

    static constexpr size_t capacity() noexcept { return Capacity; }

    /** Consumer side only. */
    void clear() noexcept { tail.store (head.load (std::memory_order_acquire), std::memory_order_release); }

private:
    std::array<T, Capacity> buffer {};
    alignas (64) std::atomic<size_t> head { 0 };
    alignas (64) std::atomic<size_t> tail { 0 };
};
} // namespace eidolon
