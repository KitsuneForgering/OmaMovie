// Scratch benchmark (not part of the repo): wall time of VulkanCompositor::render per feature.
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/video_decoder.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
using namespace oma;
int main(int argc, char** argv) {
    auto dev = gpu::Device::create();
    if (!dev) { std::puts("no device"); return 1; }
    for (int f = 1; f < argc; ++f) {
        media::VideoDecoderOptions o; o.paths = {media::DecodePath::Software};
        auto d = media::VideoDecoder::open(argv[f], o);
        auto fr = (*d)->next();
        media::VideoFrame frame = std::move(**fr);
        compositor::LayerInput in; in.frame = &frame; in.color = (*d)->stream().video->color;
        auto comp = compositor::VulkanCompositor::create(**dev);
        struct Case { const char* name; compositor::Layer layer; };
        std::vector<Case> cases;
        compositor::Layer base; base.input = 0;
        compositor::Layer tiny = base; tiny.transform.offset_x = 100000; // off screen: upload+clear+submit only
        compositor::Layer look = base; look.color.exposure = 0.5; look.color.contrast = 0.3; look.filter.kind = compositor::FilterKind::Vignette;
        compositor::Layer blur = base; blur.sharpness = -1.0;
        compositor::Layer blur_small = base; blur_small.sharpness = -0.1;
        compositor::Layer sharpen = base; sharpen.sharpness = 1.0;
        auto lut = std::make_shared<compositor::Lut3d>(); lut->size = 33;
        for (int b = 0; b < 33; ++b) for (int g2 = 0; g2 < 33; ++g2) for (int r = 0; r < 33; ++r) {
            lut->rgb.push_back(g2 / 32.0F); lut->rgb.push_back(b / 32.0F); lut->rgb.push_back(r / 32.0F); }
        compositor::Layer cdl = base; cdl.grade.cdl.slope = {1.1, 0.9, 1.0}; cdl.grade.cdl.power = {0.9, 1.0, 1.1}; cdl.grade.cdl.saturation = 1.2;
        compositor::Layer curves = base; curves.grade.curves.master = {{0, 0}, {0.25, 0.2}, {0.5, 0.5}, {0.75, 0.8}, {1, 1}};
        curves.grade.curves.red = {{0, 0}, {0.5, 0.55}, {1, 1}}; curves.grade.curves.green = curves.grade.curves.red; curves.grade.curves.blue = curves.grade.curves.red;
        compositor::Layer lutl = base; lutl.grade.lut = lut;
        compositor::Layer all = curves; all.grade.cdl = cdl.grade.cdl; all.grade.lut = lut;
        cases = {{"off-screen (upload)", tiny}, {"plain", base}, {"look+vignette", look},
                 {"blur 10%", blur_small}, {"blur max", blur}, {"sharpen max", sharpen},
                 {"grade: CDL", cdl}, {"grade: curves (5+3x3 pts)", curves}, {"grade: LUT 33", lutl}, {"grade: all three", all}};
        std::printf("%s (%dx%d) -> 1920x1080\n", argv[f], frame.width(), frame.height());
        for (auto& c : cases) {
            compositor::RenderGraph g; g.layers.push_back(c.layer);
            std::vector<double> ms;
            for (int i = 0; i < 25; ++i) {
                auto t0 = std::chrono::steady_clock::now();
                auto r = (*comp)->render(g, std::span(&in, 1));
                auto t1 = std::chrono::steady_clock::now();
                if (!r) { std::printf("fail %s\n", r.error().summary().c_str()); return 1; }
                if (i >= 5) ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
            }
            std::ranges::sort(ms);
            std::printf("  %-22s median %7.2f ms  p90 %7.2f ms\n", c.name, ms[ms.size()/2], ms[ms.size()*9/10]);
        }
    }
}
