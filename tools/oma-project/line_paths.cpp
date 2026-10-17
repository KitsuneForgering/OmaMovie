// The JSON path of each line of a canonical dump, which project::to_json writes one member or
// element per line: `diff` reports `sequence.tracks[0].clips[1].start.value` instead of only a
// line number (CLAUDE.md §16.7, reverse engineering by minimal changes). One pass, O(lines).
#include "commands.hpp"

#include <string>

namespace {
struct Container {
    std::string path;
    bool array = false;
    int index = -1; // the last element's index, in arrays
};

// The member name at the start of `t` ("name": ...), unescaping only what a key can need, or
// none when the line is not a member.
std::optional<std::string> member_name(std::string_view t) {
    if (t.empty() || t.front() != '"')
        return std::nullopt;
    std::string name;
    for (std::size_t i = 1; i < t.size(); ++i) {
        if (t[i] == '\\' && i + 1 < t.size()) {
            name += t[++i];
        } else if (t[i] == '"') {
            return t.substr(i + 1).starts_with(": ") ? std::optional(name) : std::nullopt;
        } else {
            name += t[i];
        }
    }
    return std::nullopt;
}
} // namespace

std::vector<std::string> line_paths(const std::vector<std::string_view>& lines) {
    std::vector<std::string> out;
    out.reserve(lines.size());
    std::vector<Container> stack;
    for (std::string_view line : lines) {
        std::string_view t = line;
        while (!t.empty() && t.front() == ' ')
            t.remove_prefix(1);
        if (!t.empty() && t.back() == ',')
            t.remove_suffix(1);
        if (t == "}" || t == "]") { // a container closes: its own path
            out.push_back(stack.empty() ? std::string() : stack.back().path);
            if (!stack.empty())
                stack.pop_back();
            continue;
        }
        std::string path;
        if (!stack.empty()) {
            Container& parent = stack.back();
            if (parent.array) {
                path = parent.path + "[" + std::to_string(++parent.index) + "]";
            } else if (const auto name = member_name(t)) {
                path = parent.path.empty() ? *name : parent.path + "." + *name;
            } else {
                path = parent.path;
            }
        }
        out.push_back(path);
        if (t.ends_with('{') || t.ends_with('['))
            stack.push_back({.path = path, .array = t.ends_with('['), .index = -1});
    }
    return out;
}
