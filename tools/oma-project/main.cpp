#include "oma/project/document.hpp"
#include "oma/timeline/effects.hpp"

#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {

int usage() {
    std::fputs("usage: oma-project inspect|validate|dump <file>\n", stderr);
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3)
        return usage();
    const std::string_view command(argv[1]);
    if (command != "inspect" && command != "validate" && command != "dump")
        return usage();

    const std::filesystem::path file(argv[2]);
    auto doc = oma::project::load(file);
    if (!doc) {
        std::fprintf(stderr, "%s\n", doc.error().summary().c_str());
        return 1;
    }

    if (command == "validate") {
        std::puts("valid");
        // Kept and saved, but not rendered by this version (ADR-0016).
        if (doc->timeline) {
            for (const auto& u : oma::timeline::unknown_effects(*doc->timeline)) {
                std::fprintf(stderr,
                             "warning: clip %llu: unknown effect '%s' is kept but not rendered\n",
                             static_cast<unsigned long long>(u.clip.value()), u.definition.c_str());
            }
        }
    } else if (command == "inspect") {
        std::size_t clips = 0;
        const std::size_t tracks = doc->timeline ? doc->timeline->tracks().size() : 0;
        if (doc->timeline) {
            for (const auto& track : doc->timeline->tracks())
                clips += track.clips.size();
        }
        std::printf("format_version: %d\ncanvas: %u x %u\nmedia: %zu\nluts: %zu\n"
                    "tracks: %zu\nclips: %zu\n",
                    oma::project::kFormatVersion, doc->canvas_width, doc->canvas_height,
                    doc->media.size(), doc->luts.size(), tracks, clips);
    } else {
        auto json = oma::project::to_json(*doc, std::filesystem::absolute(file).parent_path());
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
