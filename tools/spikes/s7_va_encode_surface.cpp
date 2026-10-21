// S7 (M12 step 3): can the GPU write straight into a VA-API *encode* surface? Creates an NV12
// surface for encoding, exports it as DMA-BUF (one layer per plane), imports each plane into
// OmaMovie's Vulkan device as a writable image with the surface's own DRM modifier, clears the
// planes to known values on the GPU, hands them back to VA-API and reads them through VA-API.
// Prints what the driver accepts at each step; exit 0 only if the read-back values match.
#include "va_planes.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdio>
#include <cstring>

using va_planes::fail;
using va_planes::Plane;

namespace {

constexpr std::uint32_t kW = 1920;
constexpr std::uint32_t kH = 1080;
constexpr std::array<std::uint8_t, 3> kWant{128, 64, 191}; // Y, then U and V

} // namespace

int main() {
    auto created = oma::gpu::Device::create();
    if (!created)
        return fail("no Vulkan device") ? 0 : 2;
    const oma::gpu::Device& d = **created;
    const int drm = open(d.info().render_node.c_str(), O_RDWR);
    VADisplay va = vaGetDisplayDRM(drm);
    int major = 0;
    int minor = 0;
    if (drm < 0 || vaInitialize(va, &major, &minor) != VA_STATUS_SUCCESS)
        return fail("vaInitialize") ? 0 : 2;
    std::printf("device: %s, VA-API %d.%d (%s)\n", d.info().name.c_str(), major, minor, vaQueryVendorString(va));

    std::array<VASurfaceAttrib, 2> attribs{};
    attribs[0] = {.type = VASurfaceAttribPixelFormat, .flags = VA_SURFACE_ATTRIB_SETTABLE,
                  .value = {.type = VAGenericValueTypeInteger, .value = {.i = VA_FOURCC_NV12}}};
    attribs[1] = {.type = VASurfaceAttribUsageHint, .flags = VA_SURFACE_ATTRIB_SETTABLE,
                  .value = {.type = VAGenericValueTypeInteger,
                            .value = {.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER}}};
    VASurfaceID surface = VA_INVALID_SURFACE;
    if (vaCreateSurfaces(va, VA_RT_FORMAT_YUV420, kW, kH, &surface, 1, attribs.data(), 2) != VA_STATUS_SUCCESS)
        return fail("vaCreateSurfaces (NV12, encoder hint)") ? 0 : 1;
    VADRMPRIMESurfaceDescriptor desc{};
    if (vaExportSurfaceHandle(va, surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                              VA_EXPORT_SURFACE_READ_WRITE | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                              &desc) != VA_STATUS_SUCCESS)
        return fail("vaExportSurfaceHandle (read-write, separate layers)") ? 0 : 1;
    const std::uint64_t modifier = desc.objects[0].drm_format_modifier;
    std::printf("surface: %u objects, %u layers, modifier 0x%llx, Y pitch %u, UV offset %u\n", desc.num_objects,
                desc.num_layers, static_cast<unsigned long long>(modifier), desc.layers[0].pitch[0],
                desc.layers[1].offset[0]);
    for (const auto& [format, name] : {std::pair{VK_FORMAT_R8_UNORM, "R8"}, std::pair{VK_FORMAT_R8G8_UNORM, "RG8"}}) {
        std::printf("  %s with this modifier: transfer-dst %s, storage %s\n", name,
                    va_planes::supported(d, format, modifier, VK_IMAGE_USAGE_TRANSFER_DST_BIT) ? "yes" : "no",
                    va_planes::supported(d, format, modifier, VK_IMAGE_USAGE_STORAGE_BIT) ? "yes" : "no");
    }
    std::array<Plane, 2> planes;
    if (!va_planes::import_plane(d, desc, 0, VK_FORMAT_R8_UNORM, kW, kH, planes[0]) ||
        !va_planes::import_plane(d, desc, 1, VK_FORMAT_R8G8_UNORM, kW / 2, kH / 2, planes[1]))
        return 1;

    if (!va_planes::clear_planes(d, planes, kWant[0], kWant[1], kWant[2]))
        return 1;

    // Read back through VA-API, as the encoder would see the surface.
    vaSyncSurface(va, surface);
    VAImage image{};
    void* data = nullptr;
    if (vaDeriveImage(va, surface, &image) != VA_STATUS_SUCCESS ||
        vaMapBuffer(va, image.buf, &data) != VA_STATUS_SUCCESS)
        return fail("vaDeriveImage/vaMapBuffer") ? 0 : 1;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    const std::uint8_t y = bytes[image.offsets[0] + (image.pitches[0] * (kH / 2)) + (kW / 2)];
    const std::uint8_t* uv = bytes + image.offsets[1] + (image.pitches[1] * (kH / 4)) + (kW / 2);
    std::printf("read back through VA-API: Y %u, U %u, V %u (want %u %u %u)\n", y, uv[0], uv[1], kWant[0],
                kWant[1], kWant[2]);
    const bool ok = y == kWant[0] && uv[0] == kWant[1] && uv[1] == kWant[2];
    vaUnmapBuffer(va, image.buf);
    vaDestroyImage(va, image.image_id);
    for (Plane& p : planes)
        va_planes::destroy(d, p);
    for (std::uint32_t i = 0; i < desc.num_objects; ++i)
        close(desc.objects[i].fd);
    vaDestroySurfaces(va, &surface, 1);
    vaTerminate(va);
    close(drm);
    std::printf("%s\n", ok ? "PASS: the GPU wrote the encode surface VA-API reads" : "FAIL: values differ");
    return ok ? 0 : 1;
}
