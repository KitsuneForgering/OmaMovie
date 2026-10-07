#pragma once

#include "oma/base/error.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// A disk cache of opaque byte entries (ADR-0009). Never a source of truth: a miss, a corrupt
// entry or a deleted folder only costs a recomputation.
//
// Each entry is one file named by a 128-bit hash of its full key; the file repeats the key and
// checks the payload, so a hash collision, an old format or a torn write reads as a miss.
// Writes go to a temporary file renamed into place. Any thread may use one instance: the only
// shared state is the budget.
namespace oma {

class DiskCache {
public:
    // Entries larger than this are refused on write and treated as corrupt on read.
    static constexpr std::uint64_t kMaxEntryBytes = std::uint64_t{256} << 20;
    static constexpr std::size_t kMaxKeyBytes = 4096;

    DiskCache(std::filesystem::path dir, std::uint64_t budget_bytes);

    // $XDG_CACHE_HOME/omamovie, else ~/.cache/omamovie; empty without an absolute either.
    [[nodiscard]] static std::filesystem::path default_root();

    [[nodiscard]] const std::filesystem::path& dir() const noexcept { return dir_; }
    [[nodiscard]] std::uint64_t budget() const noexcept {
        return budget_.load(std::memory_order_relaxed);
    }
    void set_budget(std::uint64_t bytes) noexcept {
        budget_.store(bytes, std::memory_order_relaxed);
    }

    // The payload stored under `key`, or nothing. A hit refreshes the entry's age for eviction.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> get(std::string_view key) const;
    // Stores `bytes` under `key`, replacing an older entry.
    [[nodiscard]] Result<void> put(std::string_view key, std::span<const std::uint8_t> bytes) const;
    // Removes the least recently used entries until the store is under 90% of the budget, and
    // temporary files left by a crash. Returns the bytes removed. O(n log n) in entries.
    [[nodiscard]] Result<std::uint64_t> evict() const;
    // Removes every entry (the user's "Clear cache"); entries written meanwhile may survive.
    [[nodiscard]] Result<std::uint64_t> clear() const;
    // Bytes the entries take now (an O(n) directory scan).
    [[nodiscard]] std::uint64_t size() const;

private:
    [[nodiscard]] std::filesystem::path entry_path(std::string_view key) const;
    // Oldest entries out until at most `down_to` bytes remain, once more than `over` are stored.
    [[nodiscard]] Result<std::uint64_t> evict(std::uint64_t over, std::uint64_t down_to) const;

    std::filesystem::path dir_;
    std::atomic<std::uint64_t> budget_;
};

} // namespace oma
