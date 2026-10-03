// S4: offscreen Qt Quick sampling the compositor's RGBA16F image on OmaMovie's device.
// This diagnostic deliberately runs all queue users sequentially. It is not a UI render loop
// or a display transform. See Docs/spikes/S4-qt-shared-device.md for scope and reproduction.
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/gpu/resources.hpp"

#include <QGuiApplication>
#include <QQuickGraphicsDevice>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickRenderTarget>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QVulkanInstance>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>

namespace {

std::atomic<unsigned> validation_errors = 0;

VKAPI_ATTR VkBool32 VKAPI_CALL validation_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                  void*) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ++validation_errors;
    }
    std::fprintf(stderr, "validation: %s\n", data->pMessage);
    return VK_FALSE;
}

class ImageItem final : public QQuickItem {
public:
    explicit ImageItem(QQuickItem* parent, std::unique_ptr<QRhiTexture> texture)
        : QQuickItem(parent), texture_(std::move(texture)) {
        setFlag(ItemHasContents);
    }

protected:
    QSGNode* updatePaintNode(QSGNode* old, UpdatePaintNodeData*) override {
        auto* node = static_cast<QSGSimpleTextureNode*>(old);
        if (!node) {
            node = new QSGSimpleTextureNode;
            auto* texture = window()->createTextureFromRhiTexture(texture_.get());
            if (!texture) {
                delete node;
                return nullptr;
            }
            node->setTexture(texture);
            node->setOwnsTexture(true); // owns QRhi wrapper, NOT the compositor's VkImage
            texture_.release();
        }
        node->setRect(boundingRect());
        return node;
    }

private:
    std::unique_ptr<QRhiTexture> texture_;
};

bool check_pixels(QRhi& rhi, QRhiTexture& target, const std::array<float, 4>& expected) {
    QRhiReadbackResult pixels;
    bool completed = false;
    pixels.completed = [&] {
        completed = true;
    };
    QRhiCommandBuffer* commands = nullptr;
    if (rhi.beginOffscreenFrame(&commands) != QRhi::FrameOpSuccess) {
        return false;
    }
    auto* updates = rhi.nextResourceUpdateBatch();
    updates->readBackTexture(QRhiReadbackDescription(&target), &pixels);
    commands->resourceUpdate(updates);
    if (rhi.endOffscreenFrame() != QRhi::FrameOpSuccess || rhi.finish() != QRhi::FrameOpSuccess ||
        !completed || pixels.format != QRhiTexture::RGBA8 ||
        pixels.pixelSize != target.pixelSize() ||
        pixels.data.size() != pixels.pixelSize.width() * pixels.pixelSize.height() * 4) {
        return false;
    }
    // Diagnostic readback only: verify every pixel, outside the image-sharing path.
    for (qsizetype i = 0; i < pixels.data.size(); ++i) {
        const int actual = static_cast<unsigned char>(pixels.data[i]);
        const int wanted =
            static_cast<int>(std::lround(expected[static_cast<size_t>(i % 4)] * 255));
        if (std::abs(actual - wanted) > 2) {
            std::fprintf(stderr, "pixel mismatch at byte %lld: %d != %d\n",
                         static_cast<long long>(i), actual, wanted);
            return false;
        }
    }
    return true;
}

bool run_scene(const oma::gpu::Device& device, QVulkanInstance& instance,
               oma::compositor::VulkanCompositor& compositor, QSize size) {
    // Qt cannot retrieve flagged queues. Fail before executing an invalid Vulkan call.
    if (device.queue_create_flags() != 0) {
        std::fprintf(stderr, "Qt 6.11 requires queues created with zero flags\n");
        return false;
    }
    QQuickRenderControl control;
    QQuickWindow window(&control);
    window.setVulkanInstance(&instance);
    window.setGraphicsDevice(QQuickGraphicsDevice::fromDeviceObjects(
        device.physical_device(), device.device(), device.graphics_family(), 0));
    window.setGeometry(0, 0, size.width(), size.height());
    if (!control.initialize()) {
        std::fprintf(stderr, "QQuickRenderControl initialization failed\n");
        return false;
    }
    auto* rhi = control.rhi();
    const auto* handles = static_cast<const QRhiVulkanNativeHandles*>(rhi->nativeHandles());
    if (rhi->backend() != QRhi::Vulkan || !handles || handles->dev != device.device() ||
        handles->physDev != device.physical_device() ||
        handles->gfxQueue != device.queue(device.graphics_family(), 0)) {
        std::fprintf(stderr, "Qt native handles differ from OmaMovie handles\n");
        return false;
    }
    std::printf("Qt %s: same physical device, device and queue; %dx%d\n", qVersion(), size.width(),
                size.height());

    oma::compositor::RenderGraph graph;
    graph.width = static_cast<uint32_t>(size.width());
    graph.height = static_cast<uint32_t>(size.height());
    if (!compositor.render(graph, {})) {
        return false;
    }
    auto imported = std::unique_ptr<QRhiTexture>(rhi->newTexture(QRhiTexture::RGBA16F, size));
    const auto native_image = reinterpret_cast<quint64>(compositor.output()->handle());
    if (!imported->createFrom({native_image, VK_IMAGE_LAYOUT_GENERAL})) {
        return false;
    }
    auto target = std::unique_ptr<QRhiTexture>(
        rhi->newTexture(QRhiTexture::RGBA8, size, 1,
                        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource));
    if (!target->create()) {
        return false;
    }
    auto render_target = std::unique_ptr<QRhiTextureRenderTarget>(
        rhi->newTextureRenderTarget(QRhiTextureRenderTargetDescription(target.get())));
    auto pass = std::unique_ptr<QRhiRenderPassDescriptor>(
        render_target->newCompatibleRenderPassDescriptor());
    render_target->setRenderPassDescriptor(pass.get());
    if (!render_target->create()) {
        return false;
    }
    window.setRenderTarget(QQuickRenderTarget::fromRhiRenderTarget(render_target.get()));
    // Stack item outlives rendering; the node owns the transferred QRhi wrapper.
    auto* shared_texture = imported.get();
    ImageItem item(window.contentItem(), std::move(imported));
    item.setSize(size);
    bool ok = true;
    for (int frame = 0; frame < 30 && ok; ++frame) {
        graph.background = frame % 2 == 0 ? std::array<float, 4>{0.25F, 0.5F, 0.75F, 1.0F}
                                          : std::array<float, 4>{0.75F, 0.25F, 0.5F, 1.0F};
        // ponytail: synchronous single-thread spike, no workers or window presentation.
        // A production bridge needs a shared submission protocol and frame lifetime tracking.
        ok = compositor.render(graph, {}).has_value();
        if (!ok) {
            break;
        }
        shared_texture->setNativeLayout(VK_IMAGE_LAYOUT_GENERAL);
        item.update();
        control.polishItems();
        control.beginFrame();
        control.sync();
        control.render();
        control.endFrame();
        // No next compositor write/destruction until all Qt reads have completed.
        ok = rhi->finish() == QRhi::FrameOpSuccess;
        if (ok && (frame == 0 || frame == 29)) {
            ok = check_pixels(*rhi, *target, graph.background);
        }
    }
    // Invalidate nodes/wrappers BEFORE resizing or destroying the compositor image.
    window.setRenderTarget({});
    render_target.reset();
    pass.reset();
    target.reset();
    control.invalidate();
    std::printf("30 alternating frames, diagnostic pixel checks: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    oma::gpu::DeviceOptions options;
    options.internally_synchronized_queues = false;
    options.instance_extensions = {VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_wayland_surface",
                                   VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    options.instance_layers = {"VK_LAYER_KHRONOS_validation"};
    auto created = oma::gpu::Device::create(options);
    if (!created) {
        std::fprintf(stderr, "%s\n", created.error().summary().c_str());
        return 1;
    }
    auto device = std::move(*created);
    std::printf("%s / %s; queue flags: %u; validation enabled\n", device->info().name.c_str(),
                device->info().driver_info.c_str(), device->queue_create_flags());
    const auto create_debug = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        device->instance_proc_addr()(device->instance(), "vkCreateDebugUtilsMessengerEXT"));
    const auto destroy_debug = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        device->instance_proc_addr()(device->instance(), "vkDestroyDebugUtilsMessengerEXT"));
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    const VkDebugUtilsMessengerCreateInfoEXT debug_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = validation_message,
        .pUserData = nullptr};
    if (!create_debug || !destroy_debug ||
        create_debug(device->instance(), &debug_info, nullptr, &messenger) != VK_SUCCESS) {
        return 1;
    }
    bool ok = false;
    {
        QVulkanInstance instance;
        instance.setVkInstance(device->instance()); // borrowed: Device remains the owner
        instance.setApiVersion(QVersionNumber(1, 4));
        auto compositor = oma::compositor::VulkanCompositor::create(*device);
        const bool instance_ok = instance.create();
        if (!instance_ok) {
            std::fprintf(stderr, "Qt instance wrapper failed: %d\n", instance.errorCode());
        }
        if (!compositor) {
            std::fprintf(stderr, "%s\n", compositor.error().summary().c_str());
        }
        ok = instance_ok && compositor &&
             run_scene(*device, instance, **compositor, QSize(320, 180)) &&
             run_scene(*device, instance, **compositor, QSize(640, 360));
    } // Qt, then compositor resources; VkDevice and validation callback still alive
    const VkInstance native_instance = device->instance();
    // Device owns the instance, so the messenger must be destroyed before Device.
    destroy_debug(native_instance, messenger, nullptr);
    device.reset();
    std::printf("validation errors: %u; S4 offscreen interop: %s\n", validation_errors.load(),
                ok && validation_errors == 0 ? "PASS" : "FAIL");
    return ok && validation_errors == 0 ? 0 : 1;
}
