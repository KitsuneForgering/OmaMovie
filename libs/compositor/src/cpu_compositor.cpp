#include "oma/compositor/compositor.hpp"

#include "layer_params.hpp"
#include "look.hpp"

#include "oma/compositor/color.hpp"
#include "oma/media/video_frame.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace oma::compositor {

namespace {

// The source planes of a software frame, read like the shader's texelFetch: raw / (2^bits - 1)
// of the container.
struct Planes {
    std::array<const std::uint8_t*, 3> data{};
    std::array<int, 3> stride{};
    int container_bytes = 1;
    float container_max = 255.0F;

    [[nodiscard]] float fetch(int plane, int x, int y, int component, int components) const {
        const std::uint8_t* row =
            data[static_cast<std::size_t>(plane)] +
            (static_cast<std::ptrdiff_t>(y) * stride[static_cast<std::size_t>(plane)]);
        const std::ptrdiff_t index = (static_cast<std::ptrdiff_t>(x) * components) + component;
        if (container_bytes == 1) {
            return static_cast<float>(row[index]) / container_max;
        }
        std::uint16_t v = 0;
        std::memcpy(&v, row + (index * 2), sizeof v);
        return static_cast<float>(v) / container_max;
    }
};

float dot3(const std::array<float, 4>& r, float x, float y, float z) {
    return (r[0] * x) + (r[1] * y) + (r[2] * z);
}

float to_linear(float v, std::int32_t transfer) {
    v = std::clamp(v, 0.0F, 1.0F);
    if (transfer == static_cast<std::int32_t>(Transfer::Linear)) {
        return v;
    }
    if (transfer == static_cast<std::int32_t>(Transfer::Srgb)) {
        return v <= 0.04045F ? v / 12.92F : std::pow((v + 0.055F) / 1.055F, 2.4F);
    }
    return std::pow(v, 2.4F);
}

// The inverse of to_linear: the layer's code values again (the grading space, ADR-0012).
float from_linear(float v, std::int32_t transfer) {
    v = std::clamp(v, 0.0F, 1.0F);
    if (transfer == static_cast<std::int32_t>(Transfer::Linear)) {
        return v;
    }
    if (transfer == static_cast<std::int32_t>(Transfer::Srgb)) {
        return v <= 0.0031308F ? v * 12.92F : (1.055F * std::pow(v, 1.0F / 2.4F)) - 0.055F;
    }
    return std::pow(v, 1.0F / 2.4F);
}

struct Rgb {
    float r, g, b;
};

// Mirrors texel() in shaders/composite.comp.
Rgb texel(const LayerParams& p, const Planes& planes, int x, int y) {
    const float scale = p.misc[1];
    const float luma = planes.fetch(0, x, y, 0, 1) * scale;
    const int cx = static_cast<int>(static_cast<unsigned>(x) >> static_cast<unsigned>(p.mode[1]));
    const int cy = static_cast<int>(static_cast<unsigned>(y) >> static_cast<unsigned>(p.mode[2]));
    float cb = 0.0F;
    float cr = 0.0F;
    if (p.mode[0] == static_cast<std::int32_t>(ChromaMode::Interleaved)) {
        cb = planes.fetch(1, cx, cy, 0, 2) * scale;
        cr = planes.fetch(1, cx, cy, 1, 2) * scale;
    } else {
        cb = planes.fetch(1, cx, cy, 0, 1) * scale;
        cr = planes.fetch(2, cx, cy, 0, 1) * scale;
    }
    const float r = to_linear(dot3(p.yuv_r, luma, cb, cr) + p.yuv_r[3], p.mode[3]);
    const float g = to_linear(dot3(p.yuv_g, luma, cb, cr) + p.yuv_g[3], p.mode[3]);
    const float b = to_linear(dot3(p.yuv_b, luma, cb, cr) + p.yuv_b[3], p.mode[3]);
    return {.r = dot3(p.gamut_r, r, g, b),
            .g = dot3(p.gamut_g, r, g, b),
            .b = dot3(p.gamut_b, r, g, b)};
}

// The cropped source's texel bounds, as composite() clamps its bilinear taps: x0, y0, x1, y1.
std::array<int, 4> tap_bounds(const LayerParams& p) {
    const int width = p.extra[1];
    const int height = p.extra[2];
    return {std::clamp(static_cast<int>(std::floor(p.crop[0])), 0, width - 1),
            std::clamp(static_cast<int>(std::floor(p.crop[1])), 0, height - 1),
            std::clamp(static_cast<int>(std::ceil(p.crop[2])) - 1, 0, width - 1),
            std::clamp(static_cast<int>(std::ceil(p.crop[3])) - 1, 0, height - 1)};
}

float weight(const LayerParams& p, int i) {
    const auto k = static_cast<std::size_t>(std::abs(i));
    return p.weights[k / 4][k % 4];
}

Rgb mix(const Rgb& a, const Rgb& b, float t) {
    return {
        .r = a.r + ((b.r - a.r) * t), .g = a.g + ((b.g - a.g) * t), .b = a.b + ((b.b - a.b) * t)};
}

// The blurred source in linear light (look.hpp Detail), reduced by its factor; mirrors the
// shader's reduce, across and down passes.
struct Blurred {
    std::vector<Rgb> texels; // empty without blur or sharpen
    int width = 0;           // texels per row
    int last_x = 0;          // the last reduced texel covering the cropped source
    int last_y = 0;
};

Blurred blurred_source(const LayerParams& p, const Planes& planes) {
    if (p.detail[0] == 0.0F) {
        return {};
    }
    const auto [x0, y0, x1, y1] = tap_bounds(p);
    const int f = static_cast<int>(p.detail[3]);
    const int radius = static_cast<int>(p.detail[2]);
    Blurred out{.texels = {},
                .width = ((x1 - x0) / f) + 1,
                .last_x = (x1 - x0) / f,
                .last_y = (y1 - y0) / f};
    const auto at = [&](int x, int y) {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(out.width)) +
               static_cast<std::size_t>(x);
    };
    const std::size_t count =
        static_cast<std::size_t>(out.width) * (static_cast<std::size_t>(out.last_y) + 1);
    // Reduced across, then down, as the shader's passes 4 and 3.
    const auto rows = static_cast<std::size_t>(y1 - y0) + 1;
    std::vector<Rgb> across(static_cast<std::size_t>(out.width) * rows,
                            Rgb{.r = 0, .g = 0, .b = 0});
    const auto ff = static_cast<float>(f);
    for (int y = y0; y <= y1; ++y) {
        for (int qx = 0; qx <= out.last_x; ++qx) {
            Rgb sum{.r = 0, .g = 0, .b = 0};
            for (int i = 0; i < f; ++i) {
                const Rgb c = texel(p, planes, std::min(x0 + (qx * f) + i, x1), y);
                sum = {.r = sum.r + c.r, .g = sum.g + c.g, .b = sum.b + c.b};
            }
            across[at(qx, y - y0)] = {.r = sum.r / ff, .g = sum.g / ff, .b = sum.b / ff};
        }
    }
    std::vector<Rgb> reduced(count, Rgb{.r = 0, .g = 0, .b = 0});
    for (int qy = 0; qy <= out.last_y; ++qy) {
        for (int qx = 0; qx <= out.last_x; ++qx) {
            Rgb sum{.r = 0, .g = 0, .b = 0};
            for (int j = 0; j < f; ++j) {
                const Rgb& c = across[at(qx, std::min((qy * f) + j, y1 - y0))];
                sum = {.r = sum.r + c.r, .g = sum.g + c.g, .b = sum.b + c.b};
            }
            reduced[at(qx, qy)] = {.r = sum.r / ff, .g = sum.g / ff, .b = sum.b / ff};
        }
    }
    const auto pass = [&](const std::vector<Rgb>& in, bool down) {
        std::vector<Rgb> result(count, Rgb{.r = 0, .g = 0, .b = 0});
        for (int qy = 0; qy <= out.last_y; ++qy) {
            for (int qx = 0; qx <= out.last_x; ++qx) {
                Rgb sum{.r = 0, .g = 0, .b = 0};
                for (int i = -radius; i <= radius; ++i) {
                    const Rgb& c = down ? in[at(qx, std::clamp(qy + i, 0, out.last_y))]
                                        : in[at(std::clamp(qx + i, 0, out.last_x), qy)];
                    const float w = weight(p, i);
                    sum = {.r = sum.r + (w * c.r), .g = sum.g + (w * c.g), .b = sum.b + (w * c.b)};
                }
                result[at(qx, qy)] = sum;
            }
        }
        return result;
    };
    out.texels = pass(pass(reduced, false), true);
    return out;
}

// The blurred image at source texel (x, y); mirrors blurred_at() in shaders/composite.comp.
Rgb blurred_at(const LayerParams& p, const Blurred& b, int x, int y) {
    const auto bounds = tap_bounds(p);
    const float f = p.detail[3];
    const float ux = ((static_cast<float>(x - bounds[0]) + 0.5F) / f) - 0.5F;
    const float uy = ((static_cast<float>(y - bounds[1]) + 0.5F) / f) - 0.5F;
    const float bx = std::floor(ux);
    const float by = std::floor(uy);
    const auto qx0 = std::clamp(static_cast<int>(bx), 0, b.last_x);
    const auto qy0 = std::clamp(static_cast<int>(by), 0, b.last_y);
    const auto qx1 = std::clamp(static_cast<int>(bx) + 1, 0, b.last_x);
    const auto qy1 = std::clamp(static_cast<int>(by) + 1, 0, b.last_y);
    const auto at = [&](int qx, int qy) -> const Rgb& {
        return b.texels[(static_cast<std::size_t>(qy) * static_cast<std::size_t>(b.width)) +
                        static_cast<std::size_t>(qx)];
    };
    return mix(mix(at(qx0, qy0), at(qx1, qy0), ux - bx), mix(at(qx0, qy1), at(qx1, qy1), ux - bx),
               uy - by);
}

// The source texel after blur or sharpen; mirrors detailed() in shaders/composite.comp.
Rgb detailed(const LayerParams& p, const Planes& planes, const Blurred& blurred, int x, int y) {
    const auto mode = static_cast<DetailMode>(static_cast<int>(p.detail[0]));
    if (mode == DetailMode::None) {
        return texel(p, planes, x, y);
    }
    const Rgb b = blurred_at(p, blurred, x, y);
    if (mode == DetailMode::Blur) {
        return b;
    }
    const Rgb o = texel(p, planes, x, y);
    const float k = p.detail[1];
    return {
        .r = o.r + (k * (o.r - b.r)), .g = o.g + (k * (o.g - b.g)), .b = o.b + (k * (o.b - b.b))};
}

float smoothstep(float edge0, float edge1, float x) {
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - (2.0F * t));
}

// The layer's look at source position (sx, sy), in the order of src/look.hpp.
Rgb apply_look(const LayerParams& p, float sx, float sy, Rgb c) {
    c = {.r = std::max(c.r * p.gains[0], 0.0F),
         .g = std::max(c.g * p.gains[1], 0.0F),
         .b = std::max(c.b * p.gains[2], 0.0F)};
    if (p.gains[3] != 1.0F) {
        const auto pivot = static_cast<float>(kContrastPivot);
        const auto curve = [&](float v) {
            return pivot * std::pow(v / pivot, p.gains[3]);
        };
        c = {.r = curve(c.r), .g = curve(c.g), .b = curve(c.b)};
    }
    c = {.r = dot3(p.look_r, c.r, c.g, c.b) + p.look_r[3],
         .g = dot3(p.look_g, c.r, c.g, c.b) + p.look_g[3],
         .b = dot3(p.look_b, c.r, c.g, c.b) + p.look_b[3]};
    if (p.vignette[0] > 0.0F) {
        const float d = std::hypot(sx - p.vignette[1], sy - p.vignette[2]) * p.vignette[3];
        const float k = 1.0F - (p.vignette[0] * smoothstep(static_cast<float>(kVignetteStart),
                                                           static_cast<float>(kVignetteEnd), d));
        c = {.r = c.r * k, .g = c.g * k, .b = c.b * k};
    }
    return {.r = std::max(c.r, 0.0F), .g = std::max(c.g, 0.0F), .b = std::max(c.b, 0.0F)};
}

// Curve `c` (0 master, 1-3 red, green, blue) at x; mirrors curve() in shaders/composite.comp.
float curve(const LayerParams& p, int c, float x) {
    const int n = p.curve_count[static_cast<std::size_t>(c)];
    if (n == 0) {
        return x;
    }
    const std::size_t base = static_cast<std::size_t>(c) * kMaxCurvePoints;
    const auto& first = p.curve[base];
    const auto& last = p.curve[base + static_cast<std::size_t>(n) - 1];
    if (x <= first[0]) {
        return first[1];
    }
    if (x >= last[0]) {
        return last[1];
    }
    std::size_t k = 0;
    while (static_cast<int>(k) + 2 < n && x >= p.curve[base + k + 1][0]) {
        ++k;
    }
    const auto& p0 = p.curve[base + k];
    const auto& p1 = p.curve[base + k + 1];
    const float h = p1[0] - p0[0];
    const float t = (x - p0[0]) / h;
    const float t2 = t * t;
    const float t3 = t2 * t;
    return (((2.0F * t3) - (3.0F * t2) + 1.0F) * p0[1]) + ((t3 - (2.0F * t2) + t) * h * p0[2]) +
           (((-2.0F * t3) + (3.0F * t2)) * p1[1]) + ((t3 - t2) * h * p1[2]);
}

// Tetrahedral interpolation of the LUT; mirrors lut_sample() in shaders/composite.comp.
Rgb lut_sample(const LayerParams& p, const Lut3d& lut, Rgb e) {
    const int n = p.grade[1];
    const auto axis = [&](float v, std::size_t i) {
        return std::clamp((v - p.lut_min[i]) * p.lut_scale[i], 0.0F, static_cast<float>(n - 1));
    };
    const std::array<float, 3> s{axis(e.r, 0), axis(e.g, 1), axis(e.b, 2)};
    std::array<int, 3> i{};
    std::array<float, 3> f{};
    for (std::size_t a = 0; a < 3; ++a) {
        i[a] = std::min(static_cast<int>(std::floor(s[a])), n - 2);
        f[a] = s[a] - static_cast<float>(i[a]);
    }
    const auto at = [&](int dr, int dg, int db) {
        const std::size_t index =
            (((static_cast<std::size_t>(i[2] + db) * static_cast<std::size_t>(n)) +
              static_cast<std::size_t>(i[1] + dg)) *
             static_cast<std::size_t>(n)) +
            static_cast<std::size_t>(i[0] + dr);
        return Rgb{
            .r = lut.rgb[index * 3], .g = lut.rgb[(index * 3) + 1], .b = lut.rgb[(index * 3) + 2]};
    };
    const float fr = f[0];
    const float fg = f[1];
    const float fb = f[2];
    Rgb v1{};
    Rgb v2{};
    std::array<float, 4> w{};
    if (fr >= fg && fg >= fb) {
        v1 = at(1, 0, 0), v2 = at(1, 1, 0), w = {1.0F - fr, fr - fg, fg - fb, fb};
    } else if (fr >= fb && fb > fg) {
        v1 = at(1, 0, 0), v2 = at(1, 0, 1), w = {1.0F - fr, fr - fb, fb - fg, fg};
    } else if (fb > fr && fr >= fg) {
        v1 = at(0, 0, 1), v2 = at(1, 0, 1), w = {1.0F - fb, fb - fr, fr - fg, fg};
    } else if (fg > fr && fr >= fb) {
        v1 = at(0, 1, 0), v2 = at(1, 1, 0), w = {1.0F - fg, fg - fr, fr - fb, fb};
    } else if (fg >= fb && fb > fr) {
        v1 = at(0, 1, 0), v2 = at(0, 1, 1), w = {1.0F - fg, fg - fb, fb - fr, fr};
    } else {
        v1 = at(0, 0, 1), v2 = at(0, 1, 1), w = {1.0F - fb, fb - fg, fg - fr, fr};
    }
    const Rgb c0 = at(0, 0, 0);
    const Rgb c1 = at(1, 1, 1);
    return {.r = (w[0] * c0.r) + (w[1] * v1.r) + (w[2] * v2.r) + (w[3] * c1.r),
            .g = (w[0] * c0.g) + (w[1] * v1.g) + (w[2] * v2.g) + (w[3] * c1.g),
            .b = (w[0] * c0.b) + (w[1] * v1.b) + (w[2] * v2.b) + (w[3] * c1.b)};
}

// The layer's grade (grade.hpp); mirrors grade() in shaders/composite.comp.
Rgb apply_grade(const LayerParams& p, const Lut3d* lut, Rgb c) {
    const std::int32_t transfer = p.mode[3];
    Rgb e{.r = from_linear(c.r, transfer),
          .g = from_linear(c.g, transfer),
          .b = from_linear(c.b, transfer)};
    const std::int32_t stages = p.grade[0];
    if ((stages & kGradeCdl) != 0) {
        const auto sop = [&](float v, std::size_t i) {
            return std::pow(std::clamp((v * p.cdl_slope[i]) + p.cdl_offset[i], 0.0F, 1.0F),
                            p.cdl_power[i]);
        };
        e = {.r = sop(e.r, 0), .g = sop(e.g, 1), .b = sop(e.b, 2)};
        const float luma = (0.2126F * e.r) + (0.7152F * e.g) + (0.0722F * e.b);
        const float sat = p.cdl_slope[3];
        const auto saturate = [&](float v) {
            return std::clamp(luma + (sat * (v - luma)), 0.0F, 1.0F);
        };
        e = {.r = saturate(e.r), .g = saturate(e.g), .b = saturate(e.b)};
    }
    if ((stages & kGradeCurves) != 0) {
        e = {.r = curve(p, 1, curve(p, 0, e.r)),
             .g = curve(p, 2, curve(p, 0, e.g)),
             .b = curve(p, 3, curve(p, 0, e.b))};
    }
    if ((stages & kGradeLut) != 0 && lut != nullptr) {
        e = mix(e, lut_sample(p, *lut, e), p.lut_amount[0]);
    }
    return {.r = to_linear(e.r, transfer),
            .g = to_linear(e.g, transfer),
            .b = to_linear(e.b, transfer)};
}

// Mirrors main() in shaders/composite.comp.
void composite(const LayerParams& p, const Planes& planes, const Lut3d* lut, RgbaImage& out) {
    const auto [lo_x, lo_y, hi_x, hi_y] = tap_bounds(p);
    const Blurred blurred = blurred_source(p, planes);
    for (int oy = p.region[1]; oy < p.region[3]; ++oy) {
        for (int ox = p.region[0]; ox < p.region[2]; ++ox) {
            const float fx = static_cast<float>(ox) + 0.5F;
            const float fy = static_cast<float>(oy) + 0.5F;
            const float sx = (p.inv0[0] * fx) + (p.inv0[1] * fy) + p.inv0[2];
            const float sy = (p.inv1[0] * fx) + (p.inv1[1] * fy) + p.inv1[2];
            if (sx < p.crop[0] || sy < p.crop[1] || sx >= p.crop[2] || sy >= p.crop[3]) {
                continue;
            }
            const float bx = std::floor(sx - 0.5F);
            const float by = std::floor(sy - 0.5F);
            const float tx = (sx - 0.5F) - bx;
            const float ty = (sy - 0.5F) - by;
            const int x0 = std::clamp(static_cast<int>(bx), lo_x, hi_x);
            const int y0 = std::clamp(static_cast<int>(by), lo_y, hi_y);
            const int x1 = std::clamp(static_cast<int>(bx) + 1, lo_x, hi_x);
            const int y1 = std::clamp(static_cast<int>(by) + 1, lo_y, hi_y);
            Rgb color = apply_look(p, sx, sy,
                                   mix(mix(detailed(p, planes, blurred, x0, y0),
                                           detailed(p, planes, blurred, x1, y0), tx),
                                       mix(detailed(p, planes, blurred, x0, y1),
                                           detailed(p, planes, blurred, x1, y1), tx),
                                       ty));
            if (p.grade[0] != 0) {
                color = apply_grade(p, lut, color);
            }

            float a = p.misc[0];
            if (p.misc[2] >= 0.0F) {
                a *= std::clamp(p.misc[2] - static_cast<float>(ox), 0.0F, 1.0F);
            }
            const std::array<float, 4> src{color.r * a, color.g * a, color.b * a, a};
            float* dst =
                out.pixels.data() +
                ((static_cast<std::size_t>(oy) * out.width) + static_cast<std::size_t>(ox)) * 4;
            const float out_a = src[3] + (dst[3] * (1.0F - src[3]));
            switch (static_cast<BlendMode>(p.extra[0])) {
            case BlendMode::Add:
                for (int c = 0; c < 3; ++c) {
                    dst[c] = src[static_cast<std::size_t>(c)] + dst[c];
                }
                break;
            case BlendMode::Multiply:
                for (int c = 0; c < 3; ++c) {
                    const float s = src[static_cast<std::size_t>(c)];
                    dst[c] = (s * dst[c]) + (s * (1.0F - dst[3])) + (dst[c] * (1.0F - src[3]));
                }
                break;
            case BlendMode::Screen:
                for (int c = 0; c < 3; ++c) {
                    const float s = src[static_cast<std::size_t>(c)];
                    dst[c] = s + dst[c] - (s * dst[c]);
                }
                break;
            case BlendMode::Normal:
                for (int c = 0; c < 3; ++c) {
                    dst[c] = src[static_cast<std::size_t>(c)] + (dst[c] * (1.0F - src[3]));
                }
                break;
            }
            dst[3] = out_a;
        }
    }
}

} // namespace

std::array<float, 4> RgbaImage::at(std::uint32_t x, std::uint32_t y) const noexcept {
    const std::size_t i = ((static_cast<std::size_t>(y) * width) + x) * 4;
    return {pixels[i], pixels[i + 1], pixels[i + 2], pixels[i + 3]};
}

Result<RgbaImage> CpuCompositor::render(const RenderGraph& graph,
                                        std::span<const LayerInput> inputs) const {
    if (auto valid = validate(graph, inputs.size()); !valid) {
        return std::unexpected(valid.error());
    }
    RgbaImage out{.width = graph.width,
                  .height = graph.height,
                  .pixels =
                      std::vector<float>(static_cast<std::size_t>(graph.width) * graph.height * 4)};
    const auto bg = premultiplied_background(graph);
    for (std::size_t i = 0; i < out.pixels.size(); i += 4) {
        std::ranges::copy(bg, out.pixels.begin() + static_cast<std::ptrdiff_t>(i));
    }
    for (const Layer& layer : graph.layers) {
        const LayerInput& input = inputs[layer.input];
        if (input.frame == nullptr) {
            return make_error(ErrorCode::InvalidArgument, Category::Compositor,
                              "layer input without a frame");
        }
        const media::VideoFrame& frame = *input.frame;
        if (frame.on_gpu()) {
            return make_error(ErrorCode::Unsupported, Category::Compositor,
                              "the CPU compositor only reads software frames");
        }
        const media::SampleLayout layout = frame.layout();
        const auto prepared =
            prepare_layer(layer, input, layout, static_cast<std::uint32_t>(frame.width()),
                          static_cast<std::uint32_t>(frame.height()), graph.width, graph.height);
        if (!prepared) {
            return std::unexpected(prepared.error());
        }
        if (!prepared->visible) {
            continue;
        }
        Planes planes;
        planes.container_bytes = layout.container_bits / 8;
        planes.container_max = static_cast<float>(std::ldexp(1.0, layout.container_bits) - 1.0);
        for (int i = 0; i < layout.planes && i < 3; ++i) {
            planes.data[static_cast<std::size_t>(i)] = frame.plane(i).data();
            planes.stride[static_cast<std::size_t>(i)] = frame.stride(i);
        }
        composite(prepared->params, planes, layer.grade.lut.get(), out);
    }
    return out;
}

} // namespace oma::compositor
