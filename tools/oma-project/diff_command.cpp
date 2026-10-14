// `oma-project diff A B` (CLAUDE.md §16.7): which lines of the canonical dump differ between two
// projects, as tab-separated `±line`, JSON path and text, so a minimal change names its field. Both
// files go through load() and to_json(), so formatting and field order cannot show up as
// differences. Exit 0 when equal, 1 when they differ, 2 on an error, like diff(1).
#include "commands.hpp"

#include <cstdio>
#include <string>

namespace {
std::optional<std::string> dump(const std::filesystem::path& file) {
    auto doc = oma::project::load(file);
    auto json = doc ? oma::project::to_json(*doc, std::filesystem::absolute(file).parent_path())
                    : oma::Result<std::string>(std::unexpected(doc.error()));
    if (!json) {
        std::fprintf(stderr, "%s: %s\n", file.c_str(), json.error().summary().c_str());
        return std::nullopt;
    }
    return std::move(*json);
}

std::vector<std::string_view> lines(std::string_view text) {
    std::vector<std::string_view> out;
    for (std::size_t at = 0; at < text.size();) {
        const std::size_t end = std::min(text.find('\n', at), text.size());
        out.push_back(text.substr(at, end - at));
        at = end + 1;
    }
    return out;
}
} // namespace

int run_diff(const std::filesystem::path& a, const std::filesystem::path& b) {
    const auto left = dump(a);
    const auto right = dump(b);
    if (!left || !right)
        return 2;
    const auto a_lines = lines(*left);
    const auto b_lines = lines(*right);
    const auto edits = line_diff(a_lines, b_lines);
    if (!edits) {
        std::fprintf(stderr, "the projects differ in more than %d lines\n", kMaxEdits);
        return 2;
    }
    const auto a_paths = line_paths(a_lines);
    const auto b_paths = line_paths(b_lines);
    for (const DiffLine& d : *edits) {
        const std::string& path = (d.op == '-' ? a_paths : b_paths)[d.line - 1];
        std::printf("%c%zu\t%s\t%.*s\n", d.op, d.line, path.empty() ? "." : path.c_str(),
                    static_cast<int>(d.text.size()), d.text.data());
    }
    return edits->empty() ? 0 : 1;
}
