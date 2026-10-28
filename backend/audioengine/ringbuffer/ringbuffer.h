#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Lock-free single-producer / single-consumer ring buffer of floats.
//
// The producer (player thread) writes, the consumer (audio thread) reads.
// Indices are ever-increasing 64-bit counters; the slot is `index & mask`.
// The producer can ask the consumer to drop stale data (e.g. after a seek)
// with discardUntil(), without blocking either side.
class RingBuffer
{
public:
    // Capacity is rounded up to a power of two.
    explicit RingBuffer(size_t capacity)
    {
        size_t size = 1;
        while (size < capacity)
            size <<= 1;
        m_data.resize(size);
        m_mask = size - 1;
    }

    RingBuffer(const RingBuffer &) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;

    size_t capacity() const { return m_data.size(); }

    // ---- Producer side ----------------------------------------------------

    size_t writable() const
    {
        const uint64_t w = m_write.load(std::memory_order_relaxed);
        const uint64_t r = m_read.load(std::memory_order_acquire);
        return capacity() - size_t(w - r);
    }

    size_t write(const float *src, size_t count)
    {
        const uint64_t w = m_write.load(std::memory_order_relaxed);
        count = std::min(count, writable());

        const size_t offset = size_t(w & m_mask);
        const size_t first = std::min(count, capacity() - offset);
        std::memcpy(m_data.data() + offset, src, first * sizeof(float));
        std::memcpy(m_data.data(), src + first, (count - first) * sizeof(float));

        m_write.store(w + count, std::memory_order_release);
        return count;
    }

    uint64_t writeIndex() const { return m_write.load(std::memory_order_relaxed); }

    // Everything written before `index` is skipped by the consumer.
    void discardUntil(uint64_t index) { m_discardUntil.store(index, std::memory_order_release); }

    // ---- Consumer side ----------------------------------------------------

    size_t read(float *dst, size_t count)
    {
        uint64_t r = m_read.load(std::memory_order_relaxed);
        const uint64_t discard = m_discardUntil.load(std::memory_order_acquire);
        if (discard > r)
            r = discard;
        const uint64_t w = m_write.load(std::memory_order_acquire);
        count = std::min(count, size_t(w - r));

        const size_t offset = size_t(r & m_mask);
        const size_t first = std::min(count, capacity() - offset);
        std::memcpy(dst, m_data.data() + offset, first * sizeof(float));
        std::memcpy(dst + first, m_data.data(), (count - first) * sizeof(float));

        m_read.store(r + count, std::memory_order_release);
        return count;
    }

    // ---- Any thread -------------------------------------------------------

    // Total samples consumed so far; used to derive the playback position.
    uint64_t readIndex() const { return m_read.load(std::memory_order_acquire); }

private:
    std::vector<float> m_data;
    size_t m_mask = 0;
    alignas(64) std::atomic<uint64_t> m_write{0};
    alignas(64) std::atomic<uint64_t> m_read{0};
    alignas(64) std::atomic<uint64_t> m_discardUntil{0};
};
