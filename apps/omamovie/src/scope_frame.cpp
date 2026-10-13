#include "scope_frame.hpp"

#include "scopes.hpp"

#include <vector>

QImage small_frame(const oma::timeline::Timeline& timeline, const MediaPaths& paths,
                   const LutTables& luts, std::int64_t frame, std::int64_t tpf,
                   FrameSource& frames) {
    auto built = build_viewer_frame(timeline, paths, luts, 256, 144, frame, tpf, frames);
    if (!built)
        return {};
    std::vector<oma::compositor::LayerInput> inputs;
    for (const Picture& p : (*built)->pictures)
        inputs.push_back({.frame = p.frame.get(),
                          .color = p.color,
                          .rotation = p.rotation,
                          .sample_aspect = p.sample_aspect});
    auto rendered = oma::compositor::CpuCompositor{}.render((*built)->graph, inputs);
    return rendered ? to_srgb_image(*rendered) : QImage();
}
