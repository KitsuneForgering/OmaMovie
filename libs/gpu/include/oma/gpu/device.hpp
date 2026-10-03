#pragma once

#include "oma/base/error.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// OmaMovie's Vulkan device (CLAUDE.md §5.1, §7; Docs/spikes/S2-ffmpeg-own-device.md).
//
// libs/gpu owns the single VkInstance/VkDevice of the process. FFmpeg (libs/media), the
// compositor and Qt Quick receive its raw handles; they never create their own device. The
// public API only exposes C Vulkan handles; Vulkan-Hpp stays inside libs/gpu sources.

namespace oma::gpu {

struct QueueFamily {
    uint32_t index = 0;
    uint32_t count = 0;
    VkQueueFlags flags = 0;
    VkVideoCodecOperationFlagsKHR video_codecs = 0; // decode/encode operations, 0 if none
};

struct DeviceInfo {
    std::string name;
    std::string driver_name;
    std::string driver_info;
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    uint32_t api_version = 0;
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
};

struct DeviceOptions {
    // Pick the first device whose name contains this text (case-sensitive). Without it, the first
    // hardware device is used, falling back to a CPU implementation (e.g. lavapipe in CI).
    std::optional<std::string> name_contains;
};

// Lists the devices the loader exposes, without creating any logical device.
[[nodiscard]] Result<std::vector<DeviceInfo>> enumerate_devices();

class Device {
public:
    [[nodiscard]] static Result<std::unique_ptr<Device>> create(const DeviceOptions& options = {});

    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] const DeviceInfo& info() const noexcept;
    [[nodiscard]] std::span<const QueueFamily> queue_families() const noexcept;
    [[nodiscard]] uint32_t graphics_family() const noexcept;

    // Whether queues were created internally synchronized (VK_KHR_internally_synchronized_queues):
    // FFmpeg, the compositor and Qt can then submit to the same queue without an application lock.
    [[nodiscard]] bool internally_synchronized_queues() const noexcept;
    [[nodiscard]] VkDeviceQueueCreateFlags queue_create_flags() const noexcept;

    [[nodiscard]] bool has_extension(std::string_view name) const noexcept;
    [[nodiscard]] std::span<const char* const> enabled_extensions() const noexcept;
    [[nodiscard]] bool supports_video_decode() const noexcept;

    // Raw handles for interop. They stay valid for the lifetime of this Device.
    [[nodiscard]] VkInstance instance() const noexcept;
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept;
    [[nodiscard]] VkDevice device() const noexcept;
    [[nodiscard]] PFN_vkGetInstanceProcAddr instance_proc_addr() const noexcept;
    // The feature chain enabled at creation (FFmpeg reads it as device_features).
    [[nodiscard]] const VkPhysicalDeviceFeatures2& enabled_features() const noexcept;
    // Fetches a queue with the right creation flags (vkGetDeviceQueue2).
    [[nodiscard]] VkQueue queue(uint32_t family, uint32_t index) const;

    // Vulkan requires external synchronization of a VkQueue unless it was created internally
    // synchronized. Every submitter (FFmpeg, the compositor, Qt) brackets vkQueueSubmit* and
    // vkQueuePresent with these; they are no-ops on internally synchronized queues.
    void lock_queue(uint32_t family, uint32_t index) const;
    void unlock_queue(uint32_t family, uint32_t index) const;

private:
    struct Impl;
    explicit Device(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace oma::gpu
