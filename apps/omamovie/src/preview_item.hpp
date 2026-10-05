#pragma once

#include "viewer_frame.hpp"

#include <QQuickItem>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

class QRhiTexture;
class QSGTexture;

namespace oma::gpu {
class Device;
}
namespace oma::compositor {
class VulkanCompositor;
}

// The viewer (ui-design §5). Composites the frame with the Vulkan compositor on Qt's render
// thread and hands the result to the scene graph as a texture over the same VkImage: no
// readback, no CPU conversion (ADR-0005, CLAUDE.md §9.4).
//
// Threading and the shared queue: compositing happens in updatePaintNode, on the render thread,
// while the GUI thread is blocked and while the Qt bridge holds the graphics queue for the frame
// (the queue lock is recursive, so the compositor's own submission re-enters it). Because the
// same thread creates, resizes and destroys the swapchain, compositor submissions can never
// overlap Qt's vkDeviceWaitIdle there. Ordering on the one queue does the rest: the next encode
// waits for Qt's earlier sampling, and Qt's sampling comes after the encode.
class PreviewItem : public QQuickItem {
    Q_OBJECT
public:
    explicit PreviewItem(QQuickItem* parent = nullptr);
    ~PreviewItem() override;
    PreviewItem(const PreviewItem&) = delete;
    PreviewItem& operator=(const PreviewItem&) = delete;
    PreviewItem(PreviewItem&&) = delete;
    PreviewItem& operator=(PreviewItem&&) = delete;

    // OmaMovie's device, the one Qt renders with. Set once, before the first frame.
    void setDevice(const oma::gpu::Device* device) { device_ = device; }
    [[nodiscard]] bool hardwareDecodeAvailable() const;
    // GUI thread. nullptr shows the neutral viewer background.
    void setFrame(std::shared_ptr<const ViewerFrame> frame);
    // GUI thread: request decode and composition on Qt's render thread, so FFmpeg's Vulkan
    // work cannot overlap Qt's swapchain teardown or device-idle waits. ponytail: this blocks
    // presentation during decode; use admitted background decode if full-scene timing requires it.
    void setRequest(std::shared_ptr<const oma::timeline::Timeline> timeline,
                    std::shared_ptr<const MediaPaths> paths, std::shared_ptr<const LutTables> luts,
                    std::uint32_t width, std::uint32_t height, std::int64_t frame,
                    std::int64_t ticks_per_frame);

    // Frames composited and handed to Qt (diagnostics, the smoke check).
    [[nodiscard]] unsigned presentedFrames() const { return presented_; }
    [[nodiscard]] std::int64_t lastCompositedFrame() const {
        return last_composited_frame_.load(std::memory_order_acquire);
    }

protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData* data) override;

private:
    bool composite();       // render thread
    void releaseGpu();      // render thread: waits for the queue, then drops the GPU objects
    void waitQueueIdle() const;

    const oma::gpu::Device* device_ = nullptr;
    struct Request {
        std::shared_ptr<const oma::timeline::Timeline> timeline;
        std::shared_ptr<const MediaPaths> paths;
        std::shared_ptr<const LutTables> luts;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::int64_t frame = 0;
        std::int64_t ticks_per_frame = 0;
    };
    std::optional<Request> request_;
    std::shared_ptr<const ViewerFrame> frame_; // written on the GUI thread, read during sync
    unsigned generation_ = 0;
    std::atomic<unsigned> presented_{0};
    std::atomic<std::int64_t> last_composited_frame_{-1};

    // Render thread only. The scene-graph node owns the texture wrapper (and through it the
    // QRhiTexture borrowing the compositor's display image), so Qt releases its own objects;
    // the item owns the compositor and pilot decoder.
    unsigned composited_ = 0;
    std::unique_ptr<oma::compositor::VulkanCompositor> compositor_;
    std::unique_ptr<FrameSource> gpu_frames_;
    const void* display_ = nullptr;         // the VkImage of the last encode
    const void* wrapped_ = nullptr;         // the VkImage the node's texture wraps
    QRhiTexture* rhi_texture_ = nullptr;    // owned by the node's QSGTexture
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};
