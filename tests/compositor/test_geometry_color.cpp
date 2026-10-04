#include "oma/compositor/color.hpp"
#include "oma/compositor/geometry.hpp"
#include "oma/compositor/render_graph.hpp"

#include <array>
#include <cmath>

#include "oma_test.hpp"

using oma::ErrorCode;
using oma::Rational;
using oma::compositor::Affine;
using oma::compositor::Fit;
using oma::compositor::Layer;
using oma::compositor::Mat3;
using oma::compositor::RenderGraph;
using oma::compositor::SourceGeometry;
using oma::compositor::Transfer;

namespace {

bool near(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) <= eps;
}

bool maps(const Affine& m, double x, double y, double ex, double ey, double eps = 1e-9) {
    const auto p = m.apply(x, y);
    return near(p[0], ex, eps) && near(p[1], ey, eps);
}

void composes_and_inverts() {
    const Affine m = Affine::scale(2, 3).then(Affine::rotate(90)).then(Affine::translate(10, 20));
    // (1, 0) -> scale (2, 0) -> rotate clockwise on a y-down screen (0, 2) -> (10, 22).
    expect(maps(m, 1, 0, 10, 22)).toBeTruthy();
    const auto inv = m.inverse();
    expect(inv.has_value()).toBeTruthy();
    if (inv) {
        expect(maps(*inv, 10, 22, 1, 0)).toBeTruthy();
        expect(maps(m.then(*inv), 5, -7, 5, -7, 1e-12)).toBeTruthy();
    }
    expect(Affine::scale(0, 1).inverse().has_value()).toBeFalsy();
}

void fits_letterbox_and_fill() {
    Layer layer;
    const SourceGeometry hd{.width = 320, .height = 180};
    // Same aspect: corners land on the output corners.
    auto m = oma::compositor::source_to_output(layer, hd, 1920, 1080);
    expect(maps(m, 0, 0, 0, 0) && maps(m, 320, 180, 1920, 1080)).toBeTruthy();
    // 4:3 into 16:9: pillarbox, 1440 wide and centered.
    const SourceGeometry sd{.width = 640, .height = 480};
    m = oma::compositor::source_to_output(layer, sd, 1920, 1080);
    expect(maps(m, 0, 0, 240, 0) && maps(m, 640, 480, 1680, 1080)).toBeTruthy();
    // Fill covers the output and overflows vertically.
    layer.fit = Fit::Fill;
    m = oma::compositor::source_to_output(layer, sd, 1920, 1080);
    expect(maps(m, 0, 0, 0, -180) && maps(m, 640, 480, 1920, 1260)).toBeTruthy();
    // Native: one source pixel per output pixel, centered.
    layer.fit = Fit::Native;
    m = oma::compositor::source_to_output(layer, hd, 1920, 1080);
    expect(maps(m, 0, 0, 800, 450)).toBeTruthy();
}

void applies_display_rotation_and_aspect() {
    Layer layer;
    // A phone video stored landscape with a 90 degree CCW display rotation shows as portrait.
    const SourceGeometry phone{.width = 320, .height = 180, .rotation = 90};
    const auto m = oma::compositor::source_to_output(layer, phone, 1920, 1080);
    const auto box = oma::compositor::covered_pixels(layer, phone, 1920, 1080);
    expect(box.has_value()).toBeTruthy();
    if (box) {
        // 180x320 scaled by 1080/320 = 607.5 wide, centered: [656.25, 1263.75).
        expect((*box)[0]).toEqual(656);
        expect((*box)[2]).toEqual(1264);
        expect((*box)[1]).toEqual(0);
        expect((*box)[3]).toEqual(1080);
    }
    // CCW display rotation: the source's top-left corner ends up bottom-left.
    expect(maps(m, 0, 0, 656.25, 1080)).toBeTruthy();
    // Anamorphic: 2:1 pixels make 160x180 storage display as 320x180.
    const SourceGeometry wide{
        .width = 160, .height = 180, .sample_aspect = Rational::literal(2, 1)};
    const auto w = oma::compositor::source_to_output(layer, wide, 1920, 1080);
    expect(maps(w, 160, 180, 1920, 1080)).toBeTruthy();
}

void crops_and_transforms() {
    Layer layer;
    layer.fit = Fit::Native;
    layer.crop = {.left = 0.25, .top = 0.0, .right = 0.25, .bottom = 0.0};
    layer.transform = {.offset_x = 100, .offset_y = -50, .scale_x = 2, .scale_y = 2, .rotation = 0};
    const SourceGeometry src{.width = 400, .height = 200};
    const auto m = oma::compositor::source_to_output(layer, src, 1000, 1000);
    // Crop center (200, 100) goes to output center + offset.
    expect(maps(m, 200, 100, 600, 450)).toBeTruthy();
    // The crop's left edge is 100 source pixels left of its center: 200 output pixels.
    expect(maps(m, 100, 100, 400, 450)).toBeTruthy();
    const auto box = oma::compositor::covered_pixels(layer, src, 1000, 1000);
    expect(box && (*box)[0] == 400 && (*box)[2] == 800 && (*box)[1] == 250 && (*box)[3] == 650)
        .toBeTruthy();
    // Entirely outside the output.
    layer.transform.offset_x = 5000;
    expect(oma::compositor::covered_pixels(layer, src, 1000, 1000).has_value()).toBeFalsy();
}

void validates_graphs() {
    RenderGraph g;
    g.layers.push_back(Layer{});
    expect(oma::compositor::validate(g, 1).has_value()).toBeTruthy();
    expect(oma::compositor::validate(g, 0).has_value()).toBeFalsy();
    g.layers[0].crop.left = 0.6;
    g.layers[0].crop.right = 0.5;
    expect(oma::compositor::validate(g, 1).has_value()).toBeFalsy();
    g.layers[0].crop = {};
    g.layers[0].opacity = 1.5F;
    expect(oma::compositor::validate(g, 1).has_value()).toBeFalsy();
    g.layers[0].opacity = 1.0F;
    g.layers[0].transform.scale_x = 0.0;
    auto r = oma::compositor::validate(g, 1);
    expect(!r && r.error().code() == ErrorCode::InvalidArgument).toBeTruthy();
    g.layers[0].transform.scale_x = 1.0;
    g.layers[0].color.exposure = 4.5;
    expect(oma::compositor::validate(g, 1).has_value()).toBeFalsy();
    g.layers[0].color.exposure = -4.0;
    g.layers[0].color.saturation = 1.0;
    expect(oma::compositor::validate(g, 1).has_value()).toBeTruthy();
    g.layers[0].filter.amount = 1.2;
    expect(oma::compositor::validate(g, 1).has_value()).toBeFalsy();
    g.layers[0].filter.amount = 1.0;
    g.width = 0;
    expect(oma::compositor::validate(g, 1).has_value()).toBeFalsy();
}

std::array<double, 3> rgb(const oma::compositor::YuvToRgb& m, double y, double cb, double cr) {
    const auto v = m.matrix.apply({y, cb, cr});
    return {v[0] + m.offset[0], v[1] + m.offset[1], v[2] + m.offset[2]};
}

bool rgb_near(const std::array<double, 3>& v, double r, double g, double b, double eps = 1e-6) {
    return near(v[0], r, eps) && near(v[1], g, eps) && near(v[2], b, eps);
}

void converts_yuv() {
    oma::media::ColorInfo bt709{
        .matrix = 1, .primaries = 1, .transfer = 1, .range = oma::media::ColorRange::Limited};
    const auto m8 = oma::compositor::yuv_to_rgb(bt709, 8, 1080);
    // Limited range: 16 is black, 235 white, 128 neutral chroma.
    expect(rgb_near(rgb(m8, 16.0 / 255, 128.0 / 255, 128.0 / 255), 0, 0, 0)).toBeTruthy();
    expect(rgb_near(rgb(m8, 235.0 / 255, 128.0 / 255, 128.0 / 255), 1, 1, 1)).toBeTruthy();
    // Pure BT.709 red: Y = 63, Cb = 102, Cr = 240 (rounded studio values).
    expect(rgb_near(rgb(m8, 63.0 / 255, 102.0 / 255, 240.0 / 255), 1, 0, 0, 0.01)).toBeTruthy();
    // 10-bit limited: 64 and 940.
    const auto m10 = oma::compositor::yuv_to_rgb(bt709, 10, 1080);
    expect(rgb_near(rgb(m10, 64.0 / 1023, 512.0 / 1023, 512.0 / 1023), 0, 0, 0)).toBeTruthy();
    expect(rgb_near(rgb(m10, 940.0 / 1023, 512.0 / 1023, 512.0 / 1023), 1, 1, 1)).toBeTruthy();
    // Full range: 0 and 255.
    bt709.range = oma::media::ColorRange::Full;
    const auto full = oma::compositor::yuv_to_rgb(bt709, 8, 1080);
    expect(rgb_near(rgb(full, 1.0, 128.0 / 255, 128.0 / 255), 1, 1, 1)).toBeTruthy();
    // Unspecified matrix: BT.601 for SD, so the luma weights differ from BT.709.
    oma::media::ColorInfo unspecified;
    const auto sd = oma::compositor::yuv_to_rgb(unspecified, 8, 480);
    const auto hd = oma::compositor::yuv_to_rgb(unspecified, 8, 1080);
    expect(near(sd.matrix.at(0, 2), hd.matrix.at(0, 2), 1e-3)).toBeFalsy();
}

void converts_primaries_and_transfer() {
    const Mat3 same = oma::compositor::primaries_to_bt709(1, 1080);
    expect(near(same.at(0, 0), 1, 1e-9) && near(same.at(0, 1), 0, 1e-9) &&
           near(same.at(2, 2), 1, 1e-9))
        .toBeTruthy();
    // BT.2020 -> BT.709 (ITU-R BT.2087).
    const Mat3 m = oma::compositor::primaries_to_bt709(9, 2160);
    expect(near(m.at(0, 0), 1.6605, 1e-3) && near(m.at(0, 1), -0.5876, 1e-3) &&
           near(m.at(0, 2), -0.0728, 1e-3))
        .toBeTruthy();
    expect(near(m.at(1, 0), -0.1246, 1e-3) && near(m.at(1, 1), 1.1329, 1e-3)).toBeTruthy();
    expect(near(m.at(2, 1), -0.1006, 1e-3) && near(m.at(2, 2), 1.1187, 1e-3)).toBeTruthy();

    expect(near(oma::compositor::to_linear(0.5, Transfer::Srgb), 0.214041, 1e-6)).toBeTruthy();
    expect(near(oma::compositor::to_linear(0.5, Transfer::Bt1886), std::pow(0.5, 2.4)))
        .toBeTruthy();
    expect(near(oma::compositor::to_linear(1.7, Transfer::Linear), 1.0)).toBeTruthy();
    bool hdr = false;
    expect(oma::compositor::resolve_transfer(16, &hdr) == Transfer::Bt1886 && hdr).toBeTruthy();
    expect(oma::compositor::resolve_transfer(13, &hdr) == Transfer::Srgb && !hdr).toBeTruthy();
}

} // namespace

void run_geometry_tests() {
    describe("compositor geometry", {
        it("composes and inverts affine transforms", { composes_and_inverts(); });
        it("fits, fills and places sources natively", { fits_letterbox_and_fill(); });
        it("applies display rotation and pixel aspect", { applies_display_rotation_and_aspect(); });
        it("crops, scales and offsets layers", { crops_and_transforms(); });
        it("validates render graphs", { validates_graphs(); });
    });
}

void run_color_tests() {
    describe("compositor color", {
        it("converts limited, full and 10-bit YUV", { converts_yuv(); });
        it("converts primaries and decodes transfers", { converts_primaries_and_transfer(); });
    });
}
