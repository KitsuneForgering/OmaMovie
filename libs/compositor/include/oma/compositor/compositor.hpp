#pragma once

#include "oma/base/error.hpp"
#include "oma/base/rational.hpp"
#include "oma/compositor/render_graph.hpp"
#include "oma/media/probe.hpp"

#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace oma::gpu {
class Device;
class Image;
} // namespace oma::gpu

namespace oma::media {
class VideoFrame;
}

namespace oma::compositor {

// One source of the frame being composed: a decoded picture plus how to interpret it.
struct LayerInput {
    media::VideoFrame* frame = nullptr; // GPU or software frame; must outlive the render call
    media::ColorInfo color;
    int rotation = 0; // counterclockwise display rotation from the stream
    Rational sample_aspect = Rational::literal(1, 1);
};

// Linear-light BT.709 RGBA with premultiplied alpha, one float per channel, rows top first.
struct RgbaImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<float> pixels;

    [[nodiscard]] std::array<float, 4> at(std::uint32_t x, std::uint32_t y) const noexcept;
};

// The correctness reference (CLAUDE.md §9.3): the same per-pixel math as the GPU compositor,
// on software frames, in plain C++. Slow; for tests and GPU-less diagnostics.
class CpuCompositor {
public:
    [[nodiscard]] Result<RgbaImage> render(const RenderGraph& graph,
                                           std::span<const LayerInput> inputs) const;
};

// Executes render graphs on OmaMovie's device into an RGBA16F output image (linear,
// premultiplied), taking GPU frames without copies and uploading software frames.
// Synchronous for now: render() returns when the GPU has finished. Not thread-safe.
class VulkanCompositor {
public:
    [[nodiscard]] static Result<std::unique_ptr<VulkanCompositor>>
    create(const gpu::Device& device);

    ~VulkanCompositor();
    VulkanCompositor(const VulkanCompositor&) = delete;
    VulkanCompositor& operator=(const VulkanCompositor&) = delete;
    VulkanCompositor(VulkanCompositor&&) = delete;
    VulkanCompositor& operator=(VulkanCompositor&&) = delete;

    [[nodiscard]] Result<void> render(const RenderGraph& graph, std::span<const LayerInput> inputs);

    // The last render's result, in VK_IMAGE_LAYOUT_GENERAL. Valid until the next render.
    [[nodiscard]] const gpu::Image* output() const noexcept;

    // Copies the output back to memory (tests, thumbnails). Slow by design.
    [[nodiscard]] Result<RgbaImage> read_output();

    // Preview display transform: encodes the last render with the sRGB transfer function into
    // an RGBA8 image (VK_IMAGE_LAYOUT_GENERAL, sampleable, same size as the output) for an SDR
    // display. Returns when the GPU has finished; the image stays valid until the next call.
    // Later work on this queue (for example Qt sampling it) is ordered after the encode, and the
    // next encode waits for earlier work on the queue before overwriting it. The full SDR/HDR
    // display policy is ADR-0006's.
    [[nodiscard]] Result<const gpu::Image*> encode_display();

    // RGBA8 pixels of the last encode_display() (tests). Slow by design.
    [[nodiscard]] Result<std::vector<std::uint8_t>> read_display();

private:
    struct Impl;
    explicit VulkanCompositor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace oma::compositor
