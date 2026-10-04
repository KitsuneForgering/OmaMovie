#include "oma/gpu/device.hpp"

#include "oma/base/log.hpp"

// Vulkan-Hpp without exceptions, returning std::expected (Docs/spikes/S2-ffmpeg-own-device.md):
// <expected> must come first and VULKAN_HPP_USE_STD_EXPECTED must be defined.
#include <expected>
#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_RAII_NO_EXCEPTIONS
#define VULKAN_HPP_USE_STD_EXPECTED
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <mutex>
#include <set>

namespace oma::gpu {

namespace {

Error vk_error(vk::Result r, std::string what) {
    return {r == vk::Result::eErrorInitializationFailed || r == vk::Result::eErrorIncompatibleDriver
                ? ErrorCode::Unsupported
                : ErrorCode::Internal,
            Category::Gpu, std::move(what), vk::to_string(r)};
}

// Extensions OmaMovie enables when the device has them. Interop and synchronization come first;
// video extensions are only present on drivers that expose Vulkan Video (Docs/spikes/S1-*.md).
constexpr std::array kFeaturelessExtensions = {
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
    VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
    VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME,
    VK_KHR_VIDEO_DECODE_VP9_EXTENSION_NAME,
    VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME,
    VK_KHR_VIDEO_ENCODE_H264_EXTENSION_NAME,
    VK_KHR_VIDEO_ENCODE_H265_EXTENSION_NAME,
    VK_KHR_VIDEO_ENCODE_AV1_EXTENSION_NAME,
};

DeviceInfo to_info(const vk::raii::PhysicalDevice& pd) {
    const auto chain =
        pd.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDriverProperties>();
    const auto& p = chain.get<vk::PhysicalDeviceProperties2>().properties;
    const auto& drv = chain.get<vk::PhysicalDeviceDriverProperties>();
    return DeviceInfo{.name = p.deviceName.data(),
                      .driver_name = drv.driverName.data(),
                      .driver_info = drv.driverInfo.data(),
                      .vendor_id = p.vendorID,
                      .device_id = p.deviceID,
                      .api_version = p.apiVersion,
                      .type = static_cast<VkPhysicalDeviceType>(p.deviceType)};
}

} // namespace

struct Device::Impl {
    vk::raii::Context context;
    vk::raii::Instance instance{nullptr};
    vk::raii::PhysicalDevice physical{nullptr};
    vk::raii::Device device{nullptr};

    DeviceInfo info;
    std::vector<QueueFamily> families;
    uint32_t graphics_family = 0;
    std::vector<std::string> extension_storage;
    std::vector<const char*> extensions;
    bool internally_synchronized = false;
    // One mutex per queue, indexed [family][index]; only used without internal synchronization.
    // Recursive: the Qt bridge holds the graphics queue for a whole frame (ADR-0005), and work
    // recorded on the render thread inside that frame (the viewer's compositor) submits again.
    mutable std::vector<std::vector<std::recursive_mutex>> queue_mutexes;

    // Enabled feature chain; FFmpeg keeps pointers into it, so Impl never moves (unique_ptr).
    vk::PhysicalDeviceFeatures2 features2{};
    vk::PhysicalDeviceVulkan11Features v11{};
    vk::PhysicalDeviceVulkan12Features v12{};
    vk::PhysicalDeviceVulkan13Features v13{};
    vk::PhysicalDeviceVulkan14Features v14{};
    vk::PhysicalDeviceInternallySynchronizedQueuesFeaturesKHR isq{};
    vk::PhysicalDeviceVideoMaintenance1FeaturesKHR video_maintenance1{};
    vk::PhysicalDeviceDescriptorBufferFeaturesEXT descriptor_buffer{};

    [[nodiscard]] bool enabled(std::string_view name) const {
        return std::ranges::any_of(extensions, [&](const char* e) { return name == e; });
    }
};

Result<std::vector<DeviceInfo>> enumerate_devices() {
    const vk::raii::Context context;
    const vk::ApplicationInfo app{.pApplicationName = "OmaMovie", .apiVersion = VK_API_VERSION_1_3};
    auto instance = context.createInstance({.pApplicationInfo = &app});
    if (!instance) {
        return std::unexpected(vk_error(instance.error(), "cannot create a Vulkan instance"));
    }
    auto physicals = instance->enumeratePhysicalDevices();
    if (!physicals) {
        return std::unexpected(vk_error(physicals.error(), "cannot enumerate Vulkan devices"));
    }
    std::vector<DeviceInfo> out;
    for (const auto& pd : *physicals) {
        out.push_back(to_info(pd));
    }
    return out;
}

Result<std::unique_ptr<Device>> Device::create(const DeviceOptions& options) {
    auto impl = std::make_unique<Impl>();

    const auto loader_version = impl->context.enumerateInstanceVersion();
    if (!loader_version || *loader_version < VK_API_VERSION_1_3) {
        return make_error(ErrorCode::Unsupported, Category::Gpu, "Vulkan 1.3 loader required");
    }
    const vk::ApplicationInfo app{.pApplicationName = "OmaMovie",
                                  .applicationVersion = 1,
                                  .pEngineName = "OmaMovie",
                                  .engineVersion = 1,
                                  .apiVersion = VK_API_VERSION_1_4};
    std::vector<const char*> instance_extensions;
    std::vector<const char*> instance_layers;
    instance_extensions.reserve(options.instance_extensions.size());
    instance_layers.reserve(options.instance_layers.size());
    for (const auto& extension : options.instance_extensions) {
        instance_extensions.push_back(extension.c_str());
    }
    for (const auto& layer : options.instance_layers) {
        instance_layers.push_back(layer.c_str());
    }
    auto instance = impl->context.createInstance(
        {.pApplicationInfo = &app,
         .enabledLayerCount = static_cast<uint32_t>(instance_layers.size()),
         .ppEnabledLayerNames = instance_layers.data(),
         .enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
         .ppEnabledExtensionNames = instance_extensions.data()});
    if (!instance) {
        return std::unexpected(vk_error(instance.error(), "cannot create a Vulkan instance"));
    }
    impl->instance = std::move(*instance);

    // ---- device selection
    auto physicals = impl->instance.enumeratePhysicalDevices();
    if (!physicals || physicals->empty()) {
        return make_error(ErrorCode::Unsupported, Category::Gpu, "no Vulkan device available");
    }
    std::optional<size_t> chosen;
    for (size_t i = 0; i < physicals->size(); ++i) {
        const DeviceInfo info = to_info((*physicals)[i]);
        if (info.api_version < VK_API_VERSION_1_3) {
            continue;
        }
        if (options.name_contains) {
            if (info.name.find(*options.name_contains) != std::string::npos) {
                chosen = i;
                break;
            }
            continue;
        }
        const bool cpu = info.type == VK_PHYSICAL_DEVICE_TYPE_CPU;
        if (!chosen ||
            (!cpu && to_info((*physicals)[*chosen]).type == VK_PHYSICAL_DEVICE_TYPE_CPU)) {
            chosen = i;
        }
    }
    if (!chosen) {
        return make_error(ErrorCode::Unsupported, Category::Gpu, "no Vulkan 1.3 device matches",
                          options.name_contains.value_or(""));
    }
    impl->physical = std::move((*physicals)[*chosen]);
    impl->info = to_info(impl->physical);

    // ---- extensions
    auto ext_props = impl->physical.enumerateDeviceExtensionProperties();
    if (!ext_props) {
        return std::unexpected(vk_error(ext_props.error(), "cannot enumerate device extensions"));
    }
    std::set<std::string> available;
    for (const auto& e : *ext_props) {
        available.insert(e.extensionName.data());
    }
    const auto want = [&](const char* name) {
        if (available.contains(name)) {
            impl->extension_storage.emplace_back(name);
        }
    };
    for (const char* name : kFeaturelessExtensions) {
        want(name);
    }
    // Swapchains need VK_KHR_surface on the instance; a headless device has neither.
    if (std::ranges::contains(options.instance_extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
        want(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    // FFmpeg and libplacebo ask vkGetDeviceQueue2 for internally synchronized queues whenever
    // this extension is enabled, so it is enabled only when the queues are created that way.
    if (options.internally_synchronized_queues) {
        want(VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME);
    }
    want(VK_KHR_VIDEO_MAINTENANCE_1_EXTENSION_NAME);
    want(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
    for (const auto& s : impl->extension_storage) {
        impl->extensions.push_back(s.c_str());
    }

    // ---- features: enable exactly what the device supports
    void** tail = &impl->features2.pNext;
    const auto link = [&](auto& s) {
        *tail = &s;
        tail = &s.pNext;
    };
    link(impl->v11);
    link(impl->v12);
    link(impl->v13);
    if (impl->info.api_version >= VK_API_VERSION_1_4) {
        link(impl->v14);
    }
    if (impl->enabled(VK_KHR_INTERNALLY_SYNCHRONIZED_QUEUES_EXTENSION_NAME)) {
        link(impl->isq);
    }
    if (impl->enabled(VK_KHR_VIDEO_MAINTENANCE_1_EXTENSION_NAME)) {
        link(impl->video_maintenance1);
    }
    if (impl->enabled(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)) {
        link(impl->descriptor_buffer);
    }
    vkGetPhysicalDeviceFeatures2(static_cast<VkPhysicalDevice>(*impl->physical),
                                 &static_cast<VkPhysicalDeviceFeatures2&>(impl->features2));
    impl->internally_synchronized = options.internally_synchronized_queues &&
                                    impl->isq.internallySynchronizedQueues == vk::True;
    if (impl->v12.timelineSemaphore != vk::True || impl->v13.synchronization2 != vk::True) {
        return make_error(ErrorCode::Unsupported, Category::Gpu,
                          "timeline semaphores and synchronization2 are required", impl->info.name);
    }

    // ---- queues: every queue of every family; flag them when supported and requested
    using QueueChain =
        vk::StructureChain<vk::QueueFamilyProperties2, vk::QueueFamilyVideoPropertiesKHR>;
    std::vector<QueueChain> qprops;
    if (available.contains(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME)) {
        qprops = impl->physical.getQueueFamilyProperties2<QueueChain>();
    } else {
        for (const auto& p : impl->physical.getQueueFamilyProperties2()) {
            QueueChain c;
            // Copy only the payload: assigning the struct would overwrite the chain's pNext.
            c.get<vk::QueueFamilyProperties2>().queueFamilyProperties = p.queueFamilyProperties;
            qprops.push_back(c);
        }
    }
    const vk::DeviceQueueCreateFlags qflags =
        impl->internally_synchronized ? vk::DeviceQueueCreateFlagBits::eInternallySynchronizedKHR
                                      : vk::DeviceQueueCreateFlags{};
    std::vector<vk::DeviceQueueCreateInfo> qcis;
    std::vector<std::vector<float>> priorities(qprops.size());
    bool have_graphics = false;
    for (uint32_t i = 0; i < qprops.size(); ++i) {
        const auto& qp = qprops[i].get<vk::QueueFamilyProperties2>().queueFamilyProperties;
        const auto& video = qprops[i].get<vk::QueueFamilyVideoPropertiesKHR>();
        priorities[i].assign(qp.queueCount, 1.0F);
        qcis.push_back({.flags = qflags,
                        .queueFamilyIndex = i,
                        .queueCount = qp.queueCount,
                        .pQueuePriorities = priorities[i].data()});
        impl->families.push_back({.index = i,
                                  .count = qp.queueCount,
                                  .flags = static_cast<VkQueueFlags>(qp.queueFlags),
                                  .video_codecs = static_cast<VkVideoCodecOperationFlagsKHR>(
                                      video.videoCodecOperations)});
        if (!have_graphics && (qp.queueFlags & vk::QueueFlagBits::eGraphics) &&
            (qp.queueFlags & vk::QueueFlagBits::eCompute)) {
            impl->graphics_family = i;
            have_graphics = true;
        }
    }
    if (!have_graphics) {
        return make_error(ErrorCode::Unsupported, Category::Gpu,
                          "no graphics + compute queue family", impl->info.name);
    }

    const vk::DeviceCreateInfo dci{.pNext = &impl->features2,
                                   .queueCreateInfoCount = static_cast<uint32_t>(qcis.size()),
                                   .pQueueCreateInfos = qcis.data(),
                                   .enabledExtensionCount =
                                       static_cast<uint32_t>(impl->extensions.size()),
                                   .ppEnabledExtensionNames = impl->extensions.data()};
    auto device = impl->physical.createDevice(dci);
    if (!device) {
        return std::unexpected(vk_error(device.error(), "cannot create the Vulkan device"));
    }
    impl->device = std::move(*device);
    impl->queue_mutexes.reserve(impl->families.size());
    for (const auto& f : impl->families) {
        impl->queue_mutexes.emplace_back(f.count);
    }

    log_info(Category::Gpu,
             "device: {} ({}, {}), Vulkan {}.{}, {} extensions, internally synchronized queues: {}",
             impl->info.name, impl->info.driver_name, impl->info.driver_info,
             VK_API_VERSION_MAJOR(impl->info.api_version),
             VK_API_VERSION_MINOR(impl->info.api_version), impl->extensions.size(),
             impl->internally_synchronized ? "yes" : "no");
    return std::unique_ptr<Device>(new Device(std::move(impl)));
}

Device::Device(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Device::~Device() = default;

const DeviceInfo& Device::info() const noexcept {
    return impl_->info;
}
std::span<const QueueFamily> Device::queue_families() const noexcept {
    return impl_->families;
}
uint32_t Device::graphics_family() const noexcept {
    return impl_->graphics_family;
}
bool Device::internally_synchronized_queues() const noexcept {
    return impl_->internally_synchronized;
}
VkDeviceQueueCreateFlags Device::queue_create_flags() const noexcept {
    return impl_->internally_synchronized ? VK_DEVICE_QUEUE_CREATE_INTERNALLY_SYNCHRONIZED_BIT_KHR
                                          : 0;
}
bool Device::has_extension(std::string_view name) const noexcept {
    return impl_->enabled(name);
}
std::span<const char* const> Device::enabled_extensions() const noexcept {
    return impl_->extensions;
}
bool Device::supports_video_decode() const noexcept {
    return std::ranges::any_of(impl_->families, [](const QueueFamily& f) {
        return (f.flags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0;
    });
}
VkInstance Device::instance() const noexcept {
    return static_cast<VkInstance>(*impl_->instance);
}
VkPhysicalDevice Device::physical_device() const noexcept {
    return static_cast<VkPhysicalDevice>(*impl_->physical);
}
VkDevice Device::device() const noexcept {
    return static_cast<VkDevice>(*impl_->device);
}
PFN_vkGetInstanceProcAddr Device::instance_proc_addr() const noexcept {
    return vkGetInstanceProcAddr;
}
const VkPhysicalDeviceFeatures2& Device::enabled_features() const noexcept {
    return static_cast<const VkPhysicalDeviceFeatures2&>(impl_->features2);
}
VkQueue Device::queue(uint32_t family, uint32_t index) const {
    const VkDeviceQueueInfo2 info{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
                                  .pNext = nullptr,
                                  .flags = queue_create_flags(),
                                  .queueFamilyIndex = family,
                                  .queueIndex = index};
    VkQueue q = VK_NULL_HANDLE;
    vkGetDeviceQueue2(device(), &info, &q);
    return q;
}
void Device::lock_queue(uint32_t family, uint32_t index) const {
    if (!impl_->internally_synchronized) {
        impl_->queue_mutexes.at(family).at(index).lock();
    }
}
void Device::unlock_queue(uint32_t family, uint32_t index) const {
    if (!impl_->internally_synchronized) {
        impl_->queue_mutexes.at(family).at(index).unlock();
    }
}

} // namespace oma::gpu
