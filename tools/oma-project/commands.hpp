#pragma once

#include "oma/project/document.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

int run_command(std::string_view command, const oma::project::Document& doc,
                const std::filesystem::path& file);

// `import FILE.otio|FILE.edl [RATE]`: prints the import report (import_command.cpp); an EDL's
// timecodes count frames at RATE (e.g. 25 or 30000/1001; default 30).
int run_import(const std::filesystem::path& file, oma::Rational edl_rate);

// `convert FILE.otio|FILE.edl OUT.omamovie [RATE]`: the import with media probed, saved.
int run_convert(const std::filesystem::path& file, const std::filesystem::path& out,
                oma::Rational edl_rate);

// "25", "24000/1001": an EDL's frame rate as given on the command line; nothing otherwise.
[[nodiscard]] std::optional<oma::Rational> rate_of(std::string_view text);

// `diff A B`: the line edits between two projects' canonical dumps (diff_command.cpp).
int run_diff(const std::filesystem::path& a, const std::filesystem::path& b);

struct DiffLine {
    char op;               // '-' only in the first file, '+' only in the second
    std::string_view text; // the line, without its newline
    std::size_t line;      // 1-based, in the file it comes from
};
inline constexpr std::int32_t kMaxEdits = 2000;
// Myers' shortest edit script (line_diff.cpp); none past kMaxEdits edits.
[[nodiscard]] std::optional<std::vector<DiffLine>>
line_diff(const std::vector<std::string_view>& a, const std::vector<std::string_view>& b);
[[nodiscard]] std::vector<DiffLine> backtrack(const std::vector<std::vector<std::int32_t>>& trace,
                                              const std::vector<std::string_view>& a,
                                              const std::vector<std::string_view>& b);

// The JSON path of each line of a canonical dump, "" for the root (line_paths.cpp).
[[nodiscard]] std::vector<std::string> line_paths(const std::vector<std::string_view>& lines);
