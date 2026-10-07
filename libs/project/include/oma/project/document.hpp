#pragma once

#include "oma/base/error.hpp"
#include "oma/timeline/ids.hpp"
#include "oma/timeline/model.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The native project format (ADR-0007): a versioned JSON file, the source of truth for a
// project. This layer maps between the file and the in-memory model; the persisted shape does
// not mirror the model's structs (CLAUDE.md §14). No Qt, no FFmpeg.

namespace oma::project {

// 2 (2026-10-05): clips may carry a segmented `time_map` (ADR-0013) and an `anchor`
// (ADR-0014). Version 1 files read unchanged: they have neither.
// 3 (2026-10-06): clips may carry a `title` (ADR-0015).
// 4 (2026-10-06): a clip's `filter` and `sharpness` became an ordered `effects` list (ADR-0016).
inline constexpr int kFormatVersion = 4;

// Identifies a media file for relinking: its size and a hash of its first and last 64 KiB.
// Duration and streams travel in the MediaInfo next to it.
struct Fingerprint {
    std::uint64_t size = 0;
    std::uint64_t hash = 0; // FNV-1a 64; not cryptographic, only to tell files apart

    friend bool operator==(const Fingerprint&, const Fingerprint&) noexcept = default;
};

[[nodiscard]] Result<Fingerprint> fingerprint_file(const std::filesystem::path& path);

// Where a moved media file went (relink, CLAUDE.md §14): a file under one of `roots` (searched
// in order, at most kRelinkDepth folders deep and kRelinkEntries entries per root) with the
// reference's file name, size and fingerprint. A same-named file with other content is not it.
inline constexpr int kRelinkDepth = 4;
inline constexpr std::size_t kRelinkEntries = 20000;
[[nodiscard]] std::optional<std::filesystem::path>
find_relocated(const std::filesystem::path& original, const Fingerprint& fingerprint,
               std::span<const std::filesystem::path> roots);

// A library item. `path` is absolute; the file also stores it relative to the project, and
// load() prefers the relative one when the absolute one is gone (a moved project folder).
struct MediaRef {
    timeline::MediaInfo info;
    std::filesystem::path path;
    std::string name;
    bool audio_only = false; // placed on the audio lanes
    Fingerprint fingerprint;
};

struct LutRef {
    timeline::LutInfo info;
    std::filesystem::path path; // the .cube file, absolute
};

struct Document {
    std::vector<MediaRef> media; // the library, in its order
    std::vector<LutRef> luts;
    std::optional<timeline::Timeline> timeline; // none until the first clip sets the sequence
    timeline::TrackId storyline;                // the primary video track (ui-design §7.1)
    std::uint32_t canvas_width = 1920;
    std::uint32_t canvas_height = 1080;
    // Top-level fields this version does not know, as raw JSON, written back unchanged so a
    // project from a compatible newer version keeps them (CLAUDE.md §14).
    std::vector<std::pair<std::string, std::string>> unknown;
};

// The file's text. `project_dir` makes media paths relative.
[[nodiscard]] Result<std::string> to_json(const Document& doc,
                                          const std::filesystem::path& project_dir);

// Parses a project. Untrusted input (CLAUDE.md §18): sizes, counts and depth are bounded, and
// the timeline is rebuilt through Timeline::restore, which checks every invariant. A newer
// format_version is refused, never guessed at.
[[nodiscard]] Result<Document> from_json(std::string_view json,
                                         const std::filesystem::path& project_dir);

// Atomic replacement: a temporary file in the same directory, fsync, rename over `file`,
// fsync of the directory; any failure leaves the previous file untouched.
[[nodiscard]] Result<void> save(const Document& doc, const std::filesystem::path& file);
[[nodiscard]] Result<Document> load(const std::filesystem::path& file);

} // namespace oma::project
