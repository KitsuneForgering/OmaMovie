#include "preview_item.hpp"

#include "oma/base/log.hpp"
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/gpu/resources.hpp"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <rhi/qrhi.h>

#include <vulkan/vulkan_core.h>

#include <utility>

PreviewItem::PreviewItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents);
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow* window) {
        if (window == nullptr) return;
        // Before Qt tears its device state down, on the render thread.
        connect(window, &QQuickWindow::sceneGraphInvalidated, this, [this] { releaseGpu(); },
                Qt::DirectConnection);
    });
}

PreviewItem::~PreviewItem() {
    // Normally released with the scene graph; this covers an item destroyed on its own.
    if (compositor_) releaseGpu();
}

bool PreviewItem::hardwareDecodeAvailable() const {
    return qEnvironmentVariableIsSet("OMA_PREVIEW_HARDWARE") && device_ != nullptr &&
           ((device_->info().dma_buf_import && device_->info().drm_modifiers) ||
            device_->supports_video_decode());
}

void PreviewItem::setFrame(std::shared_ptr<const ViewerFrame> frame) {
    request_.reset();
    frame_ = std::move(frame);
    ++generation_;
    update();
}

void PreviewItem::setRequest(std::shared_ptr<const oma::timeline::Timeline> timeline,
                             std::shared_ptr<const MediaPaths> paths, std::shared_ptr<const LutTables> luts,
                             std::uint32_t width, std::uint32_t height, std::int64_t frame,
                             std::int64_t ticks_per_frame) {
    request_ = Request{.timeline = std::move(timeline), .paths = std::move(paths), .luts = std::move(luts),
                       .width = width, .height = height, .frame = frame, .ticks_per_frame = ticks_per_frame};
    ++generation_;
    update();
}

void PreviewItem::waitQueueIdle() const {
    // vkQueueWaitIdle needs the queue externally synchronized; the lock is recursive, so this
    // also works inside the frame the Qt bridge holds.
    device_->lock_queue(device_->graphics_family(), 0);
    vkQueueWaitIdle(device_->queue(device_->graphics_family(), 0));
    device_->unlock_queue(device_->graphics_family(), 0);
}

void PreviewItem::releaseGpu() {
    if (device_ == nullptr || !compositor_) return;
    // Nothing may still sample or write the images being destroyed.
    waitQueueIdle();
    if (request_) frame_.reset(); // GPU frames may own Vulkan images through FFmpeg
    gpu_frames_.reset();
    compositor_.reset();
    display_ = nullptr;
    wrapped_ = nullptr;
    rhi_texture_ = nullptr;
    composited_ = 0;
}

bool PreviewItem::composite() {
    if (!compositor_) {
        auto created = oma::compositor::VulkanCompositor::create(*device_);
        if (!created) {
            oma::log_error(oma::Category::Compositor, "viewer compositor: {}", created.error().summary());
            return false;
        }
        compositor_ = std::move(*created);
    }
    if (request_) {
        if (!gpu_frames_) gpu_frames_ = std::make_unique<FrameSource>(device_);
        auto frame = build_viewer_frame(*request_->timeline, *request_->paths, *request_->luts,
                                        request_->width, request_->height, request_->frame,
                                        request_->ticks_per_frame, *gpu_frames_);
        if (!frame) {
            oma::log_error(oma::Category::Decode, "viewer decode: {}", frame.error().summary());
            return false;
        }
        frame_ = std::move(*frame);
    }
    const ViewerFrame& f = *frame_;
    if ((width_ != 0 && width_ != f.graph.width) || (height_ != 0 && height_ != f.graph.height)) {
        // The display image is about to be replaced: Qt may still sample the old one.
        waitQueueIdle();
    }
    std::vector<oma::compositor::LayerInput> inputs;
    inputs.reserve(f.pictures.size());
    for (const Picture& p : f.pictures) {
        inputs.push_back(
            {.frame = p.frame.get(), .color = p.color, .rotation = p.rotation, .sample_aspect = p.sample_aspect});
    }
    if (auto r = compositor_->render(f.graph, inputs); !r) {
        oma::log_error(oma::Category::Compositor, "viewer render: {}", r.error().summary());
        return false;
    }
    auto encoded = compositor_->encode_display();
    if (!encoded) {
        oma::log_error(oma::Category::Compositor, "viewer display transform: {}", encoded.error().summary());
        return false;
    }
    width_ = f.graph.width;
    height_ = f.graph.height;
    display_ = reinterpret_cast<const void*>((*encoded)->handle()); // NOLINT(performance-no-int-to-ptr)
    composited_ = generation_;
    last_composited_frame_.store(f.frame, std::memory_order_release);
    presented_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

QSGNode* PreviewItem::updatePaintNode(QSGNode* old, UpdatePaintNodeData* /*data*/) {
    auto* node = static_cast<QSGSimpleTextureNode*>(old);
    const auto drop = [&]() -> QSGNode* {
        delete node; // and with it the texture wrapper
        wrapped_ = nullptr;
        rhi_texture_ = nullptr;
        return nullptr;
    };
    if ((!frame_ && !request_) || device_ == nullptr) {
        if (gpu_frames_) {
            waitQueueIdle();
            gpu_frames_.reset();
        }
        return drop();
    }
    const bool encoded = composited_ != generation_;
    if (encoded && !composite()) return drop();
    if (display_ == nullptr || window()->rhi() == nullptr) return drop();
    if (node == nullptr) {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(true);
        wrapped_ = nullptr;
    }
    if (wrapped_ != display_) {
        const auto image = reinterpret_cast<quint64>(display_);
        auto* texture = window()->rhi()->newTexture(QRhiTexture::RGBA8,
                                                    QSize(static_cast<int>(width_), static_cast<int>(height_)));
        if (!texture->createFrom({image, VK_IMAGE_LAYOUT_GENERAL})) {
            delete texture;
            oma::log_error(oma::Category::Compositor, "Qt could not wrap the viewer image");
            return drop();
        }
        // The wrapper takes the QRhiTexture; the node takes the wrapper (and deletes the one it
        // replaces). The VkImage stays the compositor's.
        QSGTexture* wrapper = window()->createTextureFromRhiTexture(texture, QQuickWindow::TextureOwnsGLTexture);
        if (wrapper == nullptr) {
            delete texture;
            return drop();
        }
        wrapper->setFiltering(QSGTexture::Linear);
        node->setTexture(wrapper);
        rhi_texture_ = texture;
        wrapped_ = display_;
    } else if (encoded) {
        node->markDirty(QSGNode::DirtyMaterial);
    }
    // The encode left the image in GENERAL; Qt transitions from the layout it is told.
    if (encoded) rhi_texture_->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);
    const QSizeF fit = QSizeF(width_, height_).scaled(boundingRect().size(), Qt::KeepAspectRatio);
    node->setRect((width() - fit.width()) / 2, (height() - fit.height()) / 2, fit.width(), fit.height());
    return node;
}
