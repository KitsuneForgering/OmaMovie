#include "oma/gpu/resources.hpp"

#include "oma/gpu/device.hpp"

#include <optional>
#include <string>
#include <utility>

namespace oma::gpu {

namespace {

Error vk_failure(VkResult r, std::string what) {
    return {r == VK_ERROR_OUT_OF_DEVICE_MEMORY || r == VK_ERROR_OUT_OF_HOST_MEMORY
                ? ErrorCode::Unsupported
                : ErrorCode::Internal,
            Category::Gpu, std::move(what), "VkResult " + std::to_string(static_cast<int>(r))};
}

struct MemoryChoice {
    std::uint32_t type = 0;
    bool coherent = true;
};

// Preferred flags first, then a minimal fallback (e.g. cached readback may not exist).
std::optional<MemoryChoice> pick_memory(const Device& device, std::uint32_t allowed,
                                        MemoryUse use) {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(device.physical_device(), &props);
    const auto find = [&](VkMemoryPropertyFlags want) -> std::optional<MemoryChoice> {
        for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i) {
            const VkMemoryPropertyFlags flags = props.memoryTypes[i].propertyFlags;
            if ((allowed & (1U << i)) != 0 && (flags & want) == want) {
                return MemoryChoice{
                    .type = i, .coherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0};
            }
        }
        return std::nullopt;
    };
    switch (use) {
    case MemoryUse::GpuOnly:
        return find(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).or_else([&] { return find(0); });
    case MemoryUse::Upload:
        return find(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            .or_else([&] { return find(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT); });
    case MemoryUse::Readback:
        return find(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
            .or_else([&] { return find(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT); });
    }
    return std::nullopt;
}

} // namespace

// ---------------------------------------------------------------------------------- Buffer

Result<Buffer> Buffer::create(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage,
                              MemoryUse memory) {
    Buffer b;
    b.device_ = device.device();
    b.size_ = size;
    const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .pNext = nullptr,
                                  .flags = 0,
                                  .size = size,
                                  .usage = usage,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                  .queueFamilyIndexCount = 0,
                                  .pQueueFamilyIndices = nullptr};
    if (const VkResult r = vkCreateBuffer(b.device_, &info, nullptr, &b.buffer_); r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot create a buffer"));
    }
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(b.device_, b.buffer_, &req);
    const auto choice = pick_memory(device, req.memoryTypeBits, memory);
    if (!choice) {
        return make_error(ErrorCode::Unsupported, Category::Gpu, "no memory type for buffer");
    }
    b.coherent_ = choice->coherent;
    const VkMemoryAllocateInfo alloc{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                     .pNext = nullptr,
                                     .allocationSize = req.size,
                                     .memoryTypeIndex = choice->type};
    if (const VkResult r = vkAllocateMemory(b.device_, &alloc, nullptr, &b.memory_);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot allocate buffer memory"));
    }
    if (const VkResult r = vkBindBufferMemory(b.device_, b.buffer_, b.memory_, 0);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot bind buffer memory"));
    }
    if (memory != MemoryUse::GpuOnly) {
        if (const VkResult r = vkMapMemory(b.device_, b.memory_, 0, VK_WHOLE_SIZE, 0, &b.mapped_);
            r != VK_SUCCESS) {
            return std::unexpected(vk_failure(r, "cannot map buffer memory"));
        }
    }
    return b;
}

Buffer::Buffer(Buffer&& other) noexcept
    : device_(std::exchange(other.device_, VK_NULL_HANDLE)),
      buffer_(std::exchange(other.buffer_, VK_NULL_HANDLE)),
      memory_(std::exchange(other.memory_, VK_NULL_HANDLE)), size_(std::exchange(other.size_, 0)),
      mapped_(std::exchange(other.mapped_, nullptr)), coherent_(other.coherent_) {}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        reset();
        device_ = std::exchange(other.device_, VK_NULL_HANDLE);
        buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
        memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
        size_ = std::exchange(other.size_, 0);
        mapped_ = std::exchange(other.mapped_, nullptr);
        coherent_ = other.coherent_;
    }
    return *this;
}

Buffer::~Buffer() {
    reset();
}

void Buffer::reset() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    if (mapped_ != nullptr) {
        vkUnmapMemory(device_, memory_);
    }
    vkDestroyBuffer(device_, buffer_, nullptr);
    vkFreeMemory(device_, memory_, nullptr);
    device_ = VK_NULL_HANDLE;
    buffer_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
    mapped_ = nullptr;
}

void Buffer::flush() const noexcept {
    if (!coherent_ && mapped_ != nullptr) {
        const VkMappedMemoryRange range{.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                                        .pNext = nullptr,
                                        .memory = memory_,
                                        .offset = 0,
                                        .size = VK_WHOLE_SIZE};
        vkFlushMappedMemoryRanges(device_, 1, &range);
    }
}

void Buffer::invalidate() const noexcept {
    if (!coherent_ && mapped_ != nullptr) {
        const VkMappedMemoryRange range{.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                                        .pNext = nullptr,
                                        .memory = memory_,
                                        .offset = 0,
                                        .size = VK_WHOLE_SIZE};
        vkInvalidateMappedMemoryRanges(device_, 1, &range);
    }
}

// ----------------------------------------------------------------------------------- Image

Result<Image> Image::create(const Device& device, const ImageDesc& desc) {
    Image img;
    img.device_ = device.device();
    img.desc_ = desc;
    const VkImageCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                 .pNext = nullptr,
                                 .flags = 0,
                                 .imageType = VK_IMAGE_TYPE_2D,
                                 .format = desc.format,
                                 .extent = {.width = desc.width, .height = desc.height, .depth = 1},
                                 .mipLevels = 1,
                                 .arrayLayers = 1,
                                 .samples = VK_SAMPLE_COUNT_1_BIT,
                                 .tiling = VK_IMAGE_TILING_OPTIMAL,
                                 .usage = desc.usage,
                                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                 .queueFamilyIndexCount = 0,
                                 .pQueueFamilyIndices = nullptr,
                                 .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    if (const VkResult r = vkCreateImage(img.device_, &info, nullptr, &img.image_);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot create an image"));
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(img.device_, img.image_, &req);
    const auto choice = pick_memory(device, req.memoryTypeBits, MemoryUse::GpuOnly);
    if (!choice) {
        return make_error(ErrorCode::Unsupported, Category::Gpu, "no memory type for image");
    }
    const VkMemoryAllocateInfo alloc{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                     .pNext = nullptr,
                                     .allocationSize = req.size,
                                     .memoryTypeIndex = choice->type};
    if (const VkResult r = vkAllocateMemory(img.device_, &alloc, nullptr, &img.memory_);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot allocate image memory"));
    }
    if (const VkResult r = vkBindImageMemory(img.device_, img.image_, img.memory_, 0);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot bind image memory"));
    }
    const VkImageViewCreateInfo view{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                     .pNext = nullptr,
                                     .flags = 0,
                                     .image = img.image_,
                                     .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                     .format = desc.format,
                                     .components = {},
                                     .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                          .baseMipLevel = 0,
                                                          .levelCount = 1,
                                                          .baseArrayLayer = 0,
                                                          .layerCount = 1}};
    if (const VkResult r = vkCreateImageView(img.device_, &view, nullptr, &img.view_);
        r != VK_SUCCESS) {
        return std::unexpected(vk_failure(r, "cannot create an image view"));
    }
    return img;
}

Image::Image(Image&& other) noexcept
    : device_(std::exchange(other.device_, VK_NULL_HANDLE)),
      image_(std::exchange(other.image_, VK_NULL_HANDLE)),
      view_(std::exchange(other.view_, VK_NULL_HANDLE)),
      memory_(std::exchange(other.memory_, VK_NULL_HANDLE)), desc_(other.desc_) {}

Image& Image::operator=(Image&& other) noexcept {
    if (this != &other) {
        reset();
        device_ = std::exchange(other.device_, VK_NULL_HANDLE);
        image_ = std::exchange(other.image_, VK_NULL_HANDLE);
        view_ = std::exchange(other.view_, VK_NULL_HANDLE);
        memory_ = std::exchange(other.memory_, VK_NULL_HANDLE);
        desc_ = other.desc_;
    }
    return *this;
}

Image::~Image() {
    reset();
}

void Image::reset() noexcept {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyImageView(device_, view_, nullptr);
    vkDestroyImage(device_, image_, nullptr);
    vkFreeMemory(device_, memory_, nullptr);
    device_ = VK_NULL_HANDLE;
    image_ = VK_NULL_HANDLE;
    view_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
}

} // namespace oma::gpu
