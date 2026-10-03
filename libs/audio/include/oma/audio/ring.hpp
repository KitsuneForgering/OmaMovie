#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Lock-free single-producer/single-consumer ring of interleaved float frames (CLAUDE.md §12).
// After construction nothing allocates or blocks, so the real-time audio thread can read from it.

namespace oma::audio {

class SampleRing {
public:
    // Capacity is rounded up to a power of two frames.
    SampleRing(std::size_t capacity_frames, int channels);

    SampleRing(const SampleRing&) = delete;
    SampleRing& operator=(const SampleRing&) = delete;
    SampleRing(SampleRing&&) = delete;
    SampleRing& operator=(SampleRing&&) = delete;
    ~SampleRing() = default;

    [[nodiscard]] int channels() const noexcept { return channels_; }
    [[nodiscard]] std::size_t capacity() const noexcept { return mask_ + 1; }

    // Producer side. Writes whole frames; returns how many fit.
    std::size_t write(std::span<const float> interleaved) noexcept;
    [[nodiscard]] std::size_t writable() const noexcept;

    // Consumer side. Reads whole frames; returns how many were available.
    std::size_t read(std::span<float> interleaved) noexcept;
    [[nodiscard]] std::size_t readable() const noexcept;
    // Drops everything readable now (consumer side, e.g. when a flush was requested).
    void discard_readable() noexcept;

private:
    int channels_;
    std::size_t mask_;
    std::vector<float> data_;
    alignas(64) std::atomic<std::uint64_t> head_{0}; // frames written (producer)
    alignas(64) std::atomic<std::uint64_t> tail_{0}; // frames read (consumer)
};

} // namespace oma::audio
