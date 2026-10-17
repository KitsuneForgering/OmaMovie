#include "commands.hpp"

#include "oma/timeline/effects.hpp"

#include <charconv>
#include <cstdio>

int run_command(std::string_view command, const oma::project::Document& doc,
                const std::filesystem::path& file) {
    if (command == "validate") {
        std::puts("valid");
        // Kept and saved, but not rendered by this version (ADR-0016).
        if (doc.timeline) {
            for (const auto& u : oma::timeline::unknown_effects(*doc.timeline)) {
                std::fprintf(stderr,
                             "warning: clip %llu: unknown effect '%s' is kept but not rendered\n",
                             static_cast<unsigned long long>(u.clip.value()), u.definition.c_str());
            }
        }
    } else if (command == "inspect") {
        std::size_t clips = 0;
        const std::size_t tracks = doc.timeline ? doc.timeline->tracks().size() : 0;
        if (doc.timeline) {
            for (const auto& track : doc.timeline->tracks())
                clips += track.clips.size();
        }
        std::printf("format_version: %d\ncanvas: %u x %u\nmedia: %zu\nluts: %zu\n"
                    "tracks: %zu\nclips: %zu\n",
                    oma::project::kFormatVersion, doc.canvas_width, doc.canvas_height,
                    doc.media.size(), doc.luts.size(), tracks, clips);
    } else {
        auto json = oma::project::to_json(doc, std::filesystem::absolute(file).parent_path());
        if (!json) {
            std::fprintf(stderr, "%s\n", json.error().summary().c_str());
            return 1;
        }
        if (std::fwrite(json->data(), 1, json->size(), stdout) != json->size() ||
            std::fflush(stdout) != 0) {
            std::fputs("cannot write project dump\n", stderr);
            return 1;
        }
    }
    return 0;
}

std::optional<oma::Rational> rate_of(std::string_view text) {
    std::int64_t num = 0;
    std::int64_t den = 1;
    const auto slash = text.find('/');
    const auto read = [](std::string_view s, std::int64_t& v) {
        return std::from_chars(s.data(), s.data() + s.size(), v).ptr == s.data() + s.size();
    };
    if (!read(text.substr(0, slash), num) ||
        (slash != std::string_view::npos && !read(text.substr(slash + 1), den)))
        return std::nullopt;
    auto r = oma::Rational::make(num, den);
    return r && r->is_positive() ? std::optional(*r) : std::nullopt;
}
