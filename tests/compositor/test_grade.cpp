#include "oma/compositor/compositor.hpp"
#include "oma/compositor/grade.hpp"
#include "oma/media/video_frame.hpp"

#include "compositor_test.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "oma_test.hpp"

using oma::compositor::Cdl;
using oma::compositor::CpuCompositor;
using oma::compositor::CurvePoint;
using oma::compositor::Fit;
using oma::compositor::Grade;
using oma::compositor::Layer;
using oma::compositor::LayerInput;
using oma::compositor::Lut3d;
using oma::compositor::RenderGraph;
using oma::compositor::VulkanCompositor;

namespace {

using Rgb = std::array<float, 3>;

// A LUT sampling `f` on an n^3 grid, red fastest.
std::shared_ptr<const Lut3d> make_lut(std::uint32_t n, const std::function<Rgb(Rgb)>& f) {
    auto lut = std::make_shared<Lut3d>();
    lut->size = n;
    const auto step = [&](std::uint32_t i) {
        return static_cast<float>(i) / static_cast<float>(n - 1);
    };
    for (std::uint32_t b = 0; b < n; ++b) {
        for (std::uint32_t g = 0; g < n; ++g) {
            for (std::uint32_t r = 0; r < n; ++r) {
                const Rgb out = f({step(r), step(g), step(b)});
                lut->rgb.insert(lut->rgb.end(), out.begin(), out.end());
            }
        }
    }
    return lut;
}

// Strong saturation, a gamma and a hue twist: a nonlinear LUT that keeps greys grey.
Rgb creative(Rgb c) {
    const float luma = (0.2126F * c[0]) + (0.7152F * c[1]) + (0.0722F * c[2]);
    Rgb s{};
    for (std::size_t i = 0; i < 3; ++i) {
        s[i] = std::pow(std::clamp(luma + (1.6F * (c[i] - luma)), 0.0F, 1.0F), 0.8F);
    }
    return {(0.7F * s[0]) + (0.3F * s[1]), (0.7F * s[1]) + (0.3F * s[2]),
            (0.7F * s[2]) + (0.3F * s[0])};
}

std::string cube(std::uint32_t n, const std::string& header) {
    std::string text = header + "LUT_3D_SIZE " + std::to_string(n) + "\n";
    const auto lut = make_lut(n, [](Rgb c) { return c; });
    for (std::size_t i = 0; i < lut->rgb.size(); i += 3) {
        text += std::to_string(lut->rgb[i]) + " " + std::to_string(lut->rgb[i + 1]) + " " +
                std::to_string(lut->rgb[i + 2]) + "\n";
    }
    return text;
}

bool parses(const std::string& text) {
    return oma::compositor::parse_cube(text).has_value();
}

// The color patches (dark grey, mid grey, vivid orange, white; 80 px each) rendered natively
// with `grade` by the CPU reference, at row 90.
std::vector<std::array<float, 4>> graded_row(const LayerInput& in, const Grade& grade) {
    RenderGraph g;
    g.width = static_cast<std::uint32_t>(in.frame->width());
    g.height = static_cast<std::uint32_t>(in.frame->height());
    Layer l;
    l.fit = Fit::Native;
    l.grade = grade;
    g.layers.push_back(l);
    const std::array<LayerInput, 1> inputs{in};
    const auto out = CpuCompositor{}.render(g, inputs);
    std::vector<std::array<float, 4>> row;
    if (out) {
        for (std::uint32_t x = 0; x < g.width; ++x) {
            row.push_back(out->at(x, 90));
        }
    }
    return row;
}

float max_difference(const std::vector<std::array<float, 4>>& a,
                     const std::vector<std::array<float, 4>>& b) {
    float worst = a.size() == b.size() ? 0.0F : INFINITY;
    for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            worst = std::max(worst, std::abs(a[i][c] - b[i][c]));
        }
    }
    return worst;
}

float encode(float linear) { // BT.1886, the fixture's transfer
    return std::pow(std::clamp(linear, 0.0F, 1.0F), 1.0F / 2.4F);
}

void parses_both_cube_dialects() {
    const auto adobe = oma::compositor::parse_cube(
        cube(3, "# made by hand\nTITLE \"test\"\nDOMAIN_MIN 0 0 0\nDOMAIN_MAX 1 1 2\n\n"));
    expect(adobe.has_value()).toBeTruthy();
    if (adobe) {
        expect(static_cast<int>(adobe->size)).toEqual(3);
        expect(static_cast<int>(adobe->rgb.size())).toEqual(27 * 3);
        expect(static_cast<double>(adobe->domain_max[2])).toEqual(2.0);
        expect(static_cast<double>(adobe->rgb[3])).toBeCloseTo(0.5, 1e-6); // second row: red 0.5
    }
    const auto resolve = oma::compositor::parse_cube(cube(2, "LUT_3D_INPUT_RANGE 0.0 1.5\r\n"));
    expect(resolve.has_value()).toBeTruthy();
    if (resolve) {
        expect(static_cast<double>(resolve->domain_max[0])).toEqual(1.5);
    }
}

void refuses_malformed_cubes() {
    const std::string good = cube(2, "");
    expect(parses(good)).toBeTruthy();
    expect(parses("0 0 0\n")).toBeFalsy();                              // no size
    expect(parses(cube(2, "").substr(0, good.size() - 9))).toBeFalsy(); // a row short
    expect(parses(good + "1 1 1\n")).toBeFalsy();                       // a row too many
    expect(parses(good + "TITLE \"late\"\n")).toBeFalsy();              // keyword after the data
    expect(parses("LUT_3D_SIZE 1\n0 0 0\n")).toBeFalsy();
    expect(parses("LUT_3D_SIZE 66\n")).toBeFalsy(); // beyond kMaxLutSize
    expect(parses("LUT_3D_SIZE 2.5\n")).toBeFalsy();
    expect(parses("LUT_3D_SIZE 2\n0 0 nan\n")).toBeFalsy();
    expect(parses("LUT_3D_SIZE 2\n0 0 0 0\n")).toBeFalsy(); // four numbers
    expect(parses("LUT_3D_SIZE 2\n0 0x\n")).toBeFalsy();
    expect(parses("DOMAIN_MIN 1 1 1\nDOMAIN_MAX 0 0 0\n" + good)).toBeFalsy();
    const auto one_d = oma::compositor::parse_cube("LUT_1D_SIZE 2\n0 0 0\n1 1 1\n");
    expect(!one_d && one_d.error().code() == oma::ErrorCode::Unsupported).toBeTruthy();
    expect(parses(std::string(oma::compositor::kMaxCubeBytes + 1, '#'))).toBeFalsy();
}

void curves_pass_through_points_without_overshoot() {
    const std::vector<CurvePoint> crush{
        {.x = 0, .y = 0}, {.x = 0.1, .y = 0}, {.x = 0.15, .y = 0.6}, {.x = 1, .y = 1}};
    for (const CurvePoint& p : crush) {
        expect(oma::compositor::evaluate_curve(crush, p.x)).toBeCloseTo(p.y, 1e-12);
    }
    double previous = 0.0;
    bool monotone = true;
    for (int i = 0; i <= 1000; ++i) {
        const double y = oma::compositor::evaluate_curve(crush, i / 1000.0);
        monotone = monotone && y >= previous - 1e-12 && y >= 0.0 && y <= 1.0;
        previous = y;
    }
    expect(monotone).toBeTruthy();
    const std::vector<CurvePoint> inner{{.x = 0.2, .y = 0.3}, {.x = 0.8, .y = 0.6}};
    expect(oma::compositor::evaluate_curve(inner, 0.05)).toBeCloseTo(0.3, 1e-12); // flat beyond
    expect(oma::compositor::evaluate_curve({}, 0.42)).toBeCloseTo(0.42, 1e-12);   // identity
}

void validates_grades() {
    Grade g;
    expect(oma::compositor::validate(g).has_value()).toBeTruthy();
    g.curves.red = {{.x = 0.5, .y = 0.5}, {.x = 0.5, .y = 0.7}};
    expect(oma::compositor::validate(g).has_value()).toBeFalsy();
    g.curves.red.clear();
    g.cdl.power[1] = 0.0;
    expect(oma::compositor::validate(g).has_value()).toBeFalsy();
    g.cdl = Cdl{};
    auto bad = std::make_shared<Lut3d>();
    bad->size = 3;
    g.lut = bad; // no entries
    expect(oma::compositor::validate(g).has_value()).toBeFalsy();
}

void identity_lut_keeps_the_image() {
    auto src = decode_first("color_patches.y4m");
    if (!src.frame) {
        std::printf("    (skipped: fixture color_patches.y4m missing)\n");
        return;
    }
    const LayerInput in = src.input();
    Grade identity;
    identity.lut = make_lut(17, [](Rgb c) { return c; });
    // Tetrahedral interpolation is exact on linear functions; only float rounding remains.
    expect(static_cast<double>(max_difference(graded_row(in, identity), graded_row(in, {}))))
        .toBeLessThan(2e-5);
}

void creative_lut_keeps_greys_grey() {
    auto src = decode_first("color_patches.y4m");
    if (!src.frame) {
        return;
    }
    Grade g;
    g.lut = make_lut(33, creative);
    const auto row = graded_row(src.input(), g);
    const auto plain = graded_row(src.input(), {});
    if (row.size() < 200) {
        return;
    }
    for (const std::size_t x : {40U, 120U}) { // the two neutral patches
        expect(static_cast<double>(std::abs(row[x][0] - row[x][1]))).toBeLessThan(1e-6);
        expect(static_cast<double>(std::abs(row[x][1] - row[x][2]))).toBeLessThan(1e-6);
    }
    expect(std::abs(row[200][0] - plain[200][0]) > 0.01F).toBeTruthy(); // the orange changes
    g.lut_amount = 0.0;
    expect(static_cast<double>(max_difference(graded_row(src.input(), g), plain)))
        .toBeLessThan(1e-6);
}

void cdl_and_curves_follow_their_formulas() {
    auto src = decode_first("color_patches.y4m");
    if (!src.frame) {
        return;
    }
    const LayerInput in = src.input();
    const auto plain = graded_row(in, {});
    if (plain.size() < 200) {
        return;
    }
    const float e = encode(plain[120][1]); // mid grey, code value
    Grade g;
    g.cdl.slope = {0.5, 1.0, 1.0};
    g.cdl.offset = {0.0, 0.1, 0.0};
    g.cdl.power = {1.0, 1.0, 2.0};
    const auto graded = graded_row(in, g);
    expect(static_cast<double>(encode(graded[120][0])))
        .toBeCloseTo(static_cast<double>(e * 0.5F), 1e-4);
    expect(static_cast<double>(encode(graded[120][1])))
        .toBeCloseTo(static_cast<double>(e + 0.1F), 1e-4);
    expect(static_cast<double>(encode(graded[120][2])))
        .toBeCloseTo(static_cast<double>(e * e), 1e-4);
    // An inverting master curve turns white black and the dark grey light.
    Grade inverted;
    inverted.curves.master = {{.x = 0, .y = 1}, {.x = 1, .y = 0}};
    const auto negative = graded_row(in, inverted);
    expect(static_cast<double>(negative[280][1])).toBeLessThan(1e-4);
    expect(static_cast<double>(encode(negative[40][1])))
        .toBeCloseTo(1.0 - static_cast<double>(encode(plain[40][1])), 1e-4);
}

void gpu_matches_cpu_with_grades() {
    const oma::gpu::Device* device = compositor_test_device();
    if (device == nullptr) {
        return;
    }
    auto a = decode_first("h264_30fps_aac.mp4");
    auto b = decode_first("hevc_10bit.mp4");
    if (!a.frame || !b.frame) {
        std::printf("    (skipped: fixtures missing)\n");
        return;
    }
    const std::array<LayerInput, 2> inputs{a.input(), b.input()};
    auto vk = VulkanCompositor::create(*device);
    expect(vk.has_value()).toBeTruthy();
    if (!vk) {
        return;
    }
    RenderGraph g;
    g.width = 640;
    g.height = 360;
    Layer base;
    base.grade.cdl.slope = {1.1, 0.95, 0.9};
    base.grade.cdl.offset = {0.02, 0.0, -0.01};
    base.grade.cdl.power = {0.9, 1.0, 1.2};
    base.grade.cdl.saturation = 1.3;
    base.grade.curves.master = {
        {.x = 0, .y = 0.05}, {.x = 0.3, .y = 0.2}, {.x = 0.7, .y = 0.8}, {.x = 1, .y = 0.95}};
    base.grade.curves.blue = {{.x = 0, .y = 0}, {.x = 0.5, .y = 0.45}, {.x = 1, .y = 1}};
    Layer overlay;
    overlay.input = 1;
    overlay.fit = Fit::Fill;
    overlay.opacity = 0.6F;
    overlay.grade.lut = make_lut(33, creative);
    overlay.grade.lut_amount = 0.8;
    g.layers = {base, overlay};
    const auto check = [&](const char* what) {
        const auto cpu = CpuCompositor{}.render(g, inputs);
        expect((*vk)->render(g, inputs).has_value()).toBeTruthy();
        const auto gpu = (*vk)->read_output();
        expect(cpu && gpu).toBeTruthy();
        if (!cpu || !gpu) {
            return;
        }
        const auto diff = compare(*gpu, *cpu, 4e-3F);
        std::printf("    %s: max difference %.5f, %zu of %zu pixels over tolerance\n", what,
                    static_cast<double>(diff.max_abs), diff.over_tolerance, cpu->pixels.size() / 4);
        expect(diff.over_tolerance <= cpu->pixels.size() / 4 / 500).toBeTruthy();
    };
    check("CDL, curves and LUT");
    check("same LUT again (cached)");
    g.layers[1].grade.lut = make_lut(17, [](Rgb c) { return Rgb{c[2], c[0], c[1]}; });
    check("another LUT");
}

} // namespace

void run_grade_tests() {
    describe("compositor grading", {
        it("parses Adobe and Resolve .cube files", { parses_both_cube_dialects(); });
        it("refuses malformed .cube files", { refuses_malformed_cubes(); });
        it("passes curves through their points without overshoot",
           { curves_pass_through_points_without_overshoot(); });
        it("validates grades", { validates_grades(); });
        it("keeps the image under an identity LUT", { identity_lut_keeps_the_image(); });
        it("keeps greys grey under a creative LUT", { creative_lut_keeps_greys_grey(); });
        it("applies the CDL and curves to code values",
           { cdl_and_curves_follow_their_formulas(); });
        it("matches the CPU reference on the GPU", { gpu_matches_cpu_with_grades(); });
    });
}
