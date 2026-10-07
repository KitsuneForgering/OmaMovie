#include "oma/base/disk_cache.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "oma_test.hpp"

namespace fs = std::filesystem;
using oma::DiskCache;

namespace {

fs::path scratch(const char* name) {
    const fs::path dir =
        fs::temp_directory_path() / ("oma-cache-test-" + std::to_string(::getpid())) / name;
    fs::remove_all(dir);
    return dir;
}

std::vector<std::uint8_t> bytes(std::size_t n, std::uint8_t first) {
    std::vector<std::uint8_t> out(n);
    for (std::size_t i = 0; i < n; ++i)
        out[i] = static_cast<std::uint8_t>(first + i);
    return out;
}

bool round_trips() {
    const DiskCache cache(scratch("round"), 1 << 20);
    const auto payload = bytes(1000, 7);
    const bool stored = cache.put("thumbnail/v1 /a.mp4 1 2 3 256x144", payload).has_value();
    const auto back = cache.get("thumbnail/v1 /a.mp4 1 2 3 256x144");
    const bool other_missing = !cache.get("thumbnail/v1 /a.mp4 1 2 3 128x72").has_value();
    return stored && back && *back == payload && other_missing;
}

// The only file in the store (the entry just written).
fs::path only_entry(const DiskCache& cache) {
    for (const auto& e : fs::directory_iterator(cache.dir()))
        return e.path();
    return {};
}

bool damage_is_a_miss() {
    const DiskCache cache(scratch("damage"), 1 << 20);
    (void)cache.put("k", bytes(100, 1));
    const fs::path file = only_entry(cache);
    fs::resize_file(file, fs::file_size(file) - 1); // torn write
    const bool truncated = !cache.get("k").has_value();
    (void)cache.put("k", bytes(100, 1));
    {
        std::fstream f(file, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(-1, std::ios::end);
        f.put('\x7f'); // a flipped payload byte
    }
    const bool corrupt = !cache.get("k").has_value();
    // Another key's entry under this key's name (a hash collision) is not served.
    (void)cache.put("other", bytes(10, 3));
    fs::path other;
    for (const auto& e : fs::directory_iterator(cache.dir())) {
        if (e.path() != file)
            other = e.path();
    }
    fs::rename(other, file);
    const bool collision = !cache.get("k").has_value();
    return truncated && corrupt && collision;
}

bool evicts_least_recently_used() {
    const DiskCache cache(scratch("evict"), 3000);
    using namespace std::chrono_literals;
    const auto old = fs::file_time_type::clock::now() - 1h;
    for (int i = 0; i < 4; ++i)
        (void)cache.put("entry " + std::to_string(i), bytes(900, 0));
    // Age every entry, then use entry 0 again: it becomes the newest.
    for (const auto& e : fs::directory_iterator(cache.dir()))
        fs::last_write_time(e.path(), old);
    const bool hit = cache.get("entry 0").has_value();
    const auto removed = cache.evict();
    return hit && removed && *removed > 0 && cache.size() <= 2700 &&
           cache.get("entry 0").has_value();
}

// Clearing removes every entry, whatever the budget; the store keeps working afterwards.
bool clears_everything() {
    const DiskCache cache(scratch("clear"), 1 << 20);
    for (int i = 0; i < 3; ++i)
        (void)cache.put("entry " + std::to_string(i), bytes(100, 0));
    const auto removed = cache.clear();
    const bool empty = removed && *removed >= 300 && cache.size() == 0 && !cache.get("entry 1");
    return empty && cache.put("again", bytes(10, 1)).has_value() && cache.get("again").has_value();
}

bool default_root_follows_xdg() {
    const char* saved = std::getenv("XDG_CACHE_HOME"); // NOLINT(concurrency-mt-unsafe)
    const std::string before = saved != nullptr ? saved : "";
    ::setenv("XDG_CACHE_HOME", "/tmp/oma-xdg", 1); // NOLINT(concurrency-mt-unsafe)
    const bool xdg = DiskCache::default_root() == fs::path("/tmp/oma-xdg/omamovie");
    ::setenv("XDG_CACHE_HOME", "relative/ignored", 1); // NOLINT(concurrency-mt-unsafe)
    const bool relative = DiskCache::default_root().filename() == "omamovie" &&
                          DiskCache::default_root().parent_path().filename() == ".cache";
    if (saved != nullptr) {
        ::setenv("XDG_CACHE_HOME", before.c_str(), 1); // NOLINT(concurrency-mt-unsafe)
    } else {
        ::unsetenv("XDG_CACHE_HOME"); // NOLINT(concurrency-mt-unsafe)
    }
    return xdg && relative;
}

} // namespace

void run_disk_cache_tests() {
    describe("DiskCache (ADR-0009)", {
        it("stores and returns entries by their full key", { expect(round_trips()).toBeTruthy(); });
        it("reads torn, corrupt and colliding entries as misses",
           { expect(damage_is_a_miss()).toBeTruthy(); });
        it("evicts the least recently used entries below the budget",
           { expect(evicts_least_recently_used()).toBeTruthy(); });
        it("clears every entry on request", { expect(clears_everything()).toBeTruthy(); });
        it("lives under XDG_CACHE_HOME or ~/.cache",
           { expect(default_root_follows_xdg()).toBeTruthy(); });
    });
    fs::remove_all(fs::temp_directory_path() / ("oma-cache-test-" + std::to_string(::getpid())));
}
