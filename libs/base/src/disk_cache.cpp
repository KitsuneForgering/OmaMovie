#include "oma/base/disk_cache.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <string>
#include <system_error>

#include <unistd.h>

namespace oma {

namespace {

constexpr std::array<char, 8> kMagic{'O', 'M', 'A', 'C', 'A', 'C', 'H', '1'};
constexpr std::string_view kSuffix = ".entry";
constexpr std::string_view kTempPrefix = ".tmp-";
// A temporary file this old belongs to no running write.
constexpr auto kStaleTemp = std::chrono::hours(1);

std::uint64_t fnv1a(std::span<const std::uint8_t> bytes, std::uint64_t seed) {
    std::uint64_t h = seed;
    for (const std::uint8_t b : bytes)
        h = (h ^ b) * 0x100000001b3ULL;
    return h;
}

std::span<const std::uint8_t> bytes_of(std::string_view s) {
    return {reinterpret_cast<const std::uint8_t*>(s.data()), s.size()}; // NOLINT: byte view of text
}

Error cache_error(std::string message, const std::filesystem::path& path) {
    return {ErrorCode::IoError, Category::Cache, std::move(message), path.string()};
}

struct Header {
    std::array<char, 8> magic{};
    std::uint64_t key_bytes = 0;
    std::uint64_t payload_bytes = 0;
    std::uint64_t checksum = 0;
};

} // namespace

DiskCache::DiskCache(std::filesystem::path dir, std::uint64_t budget_bytes)
    : dir_(std::move(dir)), budget_(budget_bytes) {}

std::filesystem::path DiskCache::default_root() {
    // Read where a cache is created; OmaMovie never modifies its environment.
    const char* xdg = std::getenv("XDG_CACHE_HOME"); // NOLINT(concurrency-mt-unsafe)
    const char* home = std::getenv("HOME");          // NOLINT(concurrency-mt-unsafe)
    if (xdg != nullptr && std::filesystem::path(xdg).is_absolute())
        return std::filesystem::path(xdg) / "omamovie";
    if (home != nullptr && std::filesystem::path(home).is_absolute()) {
        return std::filesystem::path(home) / ".cache" / "omamovie";
    }
    return {};
}

std::filesystem::path DiskCache::entry_path(std::string_view key) const {
    // Two FNV-1a 64 passes with different seeds: 128 bits of name. The stored key decides.
    const auto bytes = bytes_of(key);
    return dir_ / std::format("{:016x}{:016x}{}", fnv1a(bytes, 0xcbf29ce484222325ULL),
                              fnv1a(bytes, 0x84222325cbf29ce4ULL), kSuffix);
}

std::optional<std::vector<std::uint8_t>> DiskCache::get(std::string_view key) const {
    if (dir_.empty() || key.size() > kMaxKeyBytes)
        return std::nullopt;
    const auto path = entry_path(key);
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    Header h;
    in.read(reinterpret_cast<char*>(&h), sizeof h); // NOLINT: plain header bytes
    if (!in || h.magic != kMagic || h.key_bytes != key.size() || h.payload_bytes > kMaxEntryBytes)
        return std::nullopt;
    std::string stored(key.size(), '\0');
    in.read(stored.data(), static_cast<std::streamsize>(stored.size()));
    if (!in || stored != key)
        return std::nullopt;
    std::vector<std::uint8_t> payload(h.payload_bytes);
    in.read(reinterpret_cast<char*>(payload.data()),
            static_cast<std::streamsize>(payload.size())); // NOLINT
    if (!in || in.peek() != std::char_traits<char>::eof() ||
        fnv1a(payload, 0xcbf29ce484222325ULL) != h.checksum) {
        return std::nullopt;
    }
    std::error_code ec;
    std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(),
                                     ec); // recently used
    return payload;
}

Result<void> DiskCache::put(std::string_view key, std::span<const std::uint8_t> bytes) const {
    if (dir_.empty())
        return make_error(ErrorCode::Unsupported, Category::Cache, "no cache directory");
    if (key.size() > kMaxKeyBytes || bytes.size() > kMaxEntryBytes) {
        return make_error(ErrorCode::InvalidArgument, Category::Cache, "cache entry too large");
    }
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    if (ec)
        return std::unexpected(cache_error("cannot create the cache directory", dir_));
    static std::atomic<std::uint64_t> counter{0};
    const auto final_path = entry_path(key);
    const auto temp =
        dir_ / std::format("{}{}-{}-{}", kTempPrefix, final_path.stem().string(), ::getpid(),
                           counter.fetch_add(1, std::memory_order_relaxed));
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        const Header h{.magic = kMagic,
                       .key_bytes = key.size(),
                       .payload_bytes = bytes.size(),
                       .checksum = fnv1a(bytes, 0xcbf29ce484222325ULL)};
        out.write(reinterpret_cast<const char*>(&h), sizeof h); // NOLINT
        out.write(key.data(), static_cast<std::streamsize>(key.size()));
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size())); // NOLINT
        out.close();
        if (!out) {
            std::filesystem::remove(temp, ec);
            return std::unexpected(cache_error("cannot write a cache entry", temp));
        }
    }
    std::filesystem::rename(temp, final_path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return std::unexpected(cache_error("cannot store a cache entry", final_path));
    }
    return {};
}

std::uint64_t DiskCache::size() const {
    std::uint64_t total = 0;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir_, ec)) {
        if (e.path().extension() == kSuffix)
            total += e.file_size(ec);
    }
    return total;
}

Result<std::uint64_t> DiskCache::evict() const {
    return evict(budget(), budget() / 10 * 9);
}

Result<std::uint64_t> DiskCache::clear() const {
    return evict(0, 0);
}

Result<std::uint64_t> DiskCache::evict(std::uint64_t over, std::uint64_t down_to) const {
    struct Entry {
        std::filesystem::file_time_type used;
        std::uint64_t bytes;
        std::filesystem::path path;
    };
    std::vector<Entry> entries;
    std::uint64_t total = 0;
    std::uint64_t removed = 0;
    std::error_code ec;
    const auto now = std::filesystem::file_time_type::clock::now();
    for (const auto& e : std::filesystem::directory_iterator(dir_, ec)) {
        std::error_code fe;
        const auto used = e.last_write_time(fe);
        const auto bytes = e.file_size(fe);
        if (fe)
            continue;
        if (e.path().filename().string().starts_with(kTempPrefix)) {
            if (now - used > kStaleTemp && std::filesystem::remove(e.path(), fe))
                removed += bytes;
            continue;
        }
        if (e.path().extension() != kSuffix)
            continue;
        entries.push_back({.used = used, .bytes = bytes, .path = e.path()});
        total += bytes;
    }
    if (ec && ec != std::errc::no_such_file_or_directory) {
        return std::unexpected(cache_error("cannot list the cache", dir_));
    }
    if (total <= over)
        return removed;
    std::ranges::sort(entries, {}, &Entry::used);
    for (const Entry& e : entries) {
        if (total <= down_to)
            break;
        std::error_code fe;
        if (std::filesystem::remove(e.path, fe)) {
            total -= e.bytes;
            removed += e.bytes;
        }
    }
    return removed;
}

} // namespace oma
