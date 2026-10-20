// Per-frame timeline::evaluate cost as tracks, media entries and clips grow.
// Usage: evaluate-scaling TRACKS MEDIA CLIPS_PER_TRACK. Built and swept by run.sh.
#include "scene.hpp"

#include "oma/timeline/evaluate.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <print>
#include <string_view>
#include <vector>

using namespace evaluate_scaling;

namespace {
std::uint64_t arg(std::string_view s) {
    std::uint64_t v = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    return ec == std::errc{} && end == s.data() + s.size() ? v : 0;
}

} // namespace

int main(int argc, char** argv) try {
    const std::uint64_t tracks = argc == 4 ? arg(argv[1]) : 0, media = argc == 4 ? arg(argv[2]) : 0,
                        clips = argc == 4 ? arg(argv[3]) : 0;
    Editor ed(Timeline::create(oma::frame_rates::k30, oma::Rational::make(1, 30).value()).value());
    if (tracks == 0 || media == 0 || clips == 0 || !build(ed, tracks, media, clips)) {
        return 1;
    }
    const auto frames = static_cast<std::int64_t>(clips) * kClipFrames;
    std::vector<double> ns;
    for (int rep = 0; rep < 5; ++rep) {
        const auto t0 = std::chrono::steady_clock::now();
        for (std::int64_t fr = 0; fr < frames; ++fr) {
            if (!evaluate(ed.timeline(), f(fr))) {
                return 2;
            }
        }
        const std::chrono::duration<double, std::nano> d = std::chrono::steady_clock::now() - t0;
        ns.push_back(d.count() / static_cast<double>(frames));
    }
    std::ranges::sort(ns);
    std::println("tracks={} media={} clips/track={} ns/frame median={:.0f} min={:.0f} max={:.0f}",
                 tracks, media, clips, ns[2], ns[0], ns[4]);
} catch (...) {
    return 3;
}
