#include "oma/gpu/device.hpp"

#include <cstdio>
#include <memory>
#include <string>

#include "oma_test.hpp"

using oma::ErrorCode;
using oma::gpu::Device;

namespace {

std::unique_ptr<Device> g_device;
bool g_attempted = false;

// Creates the shared device once. Machines without a Vulkan driver (e.g. CI without lavapipe)
// skip the GPU suites instead of failing them.
Device* device() {
    if (!g_attempted) {
        g_attempted = true;
        auto d = Device::create();
        if (d) {
            g_device = std::move(*d);
        } else {
            std::printf("  (skipping GPU tests: %s)\n", d.error().summary().c_str());
        }
    }
    return g_device.get();
}

} // namespace

oma::gpu::Device* gpu_test_device() {
    return device();
}

namespace {

bool family_has(const Device& d, uint32_t family, VkQueueFlags flag) {
    for (const auto& f : d.queue_families()) {
        if (f.index == family) {
            return (f.flags & flag) != 0;
        }
    }
    return false;
}

} // namespace

void run_device_tests() {
    describe("gpu::Device", {
        Device* d = device();
        if (d == nullptr) {
            return;
        }

        it("reports the selected device", {
            expect(d->info().name.empty()).toBeFalsy();
            expect(d->info().api_version >= VK_API_VERSION_1_3).toBeTruthy();
            std::printf("    device: %s (%s)\n", d->info().name.c_str(),
                        d->info().driver_info.c_str());
        });

        it("exposes valid raw handles for interop", {
            expect(d->instance() != VK_NULL_HANDLE).toBeTruthy();
            expect(d->physical_device() != VK_NULL_HANDLE).toBeTruthy();
            expect(d->device() != VK_NULL_HANDLE).toBeTruthy();
            expect(d->instance_proc_addr() != nullptr).toBeTruthy();
        });

        it("has a graphics and compute queue family", {
            const uint32_t g = d->graphics_family();
            expect(family_has(*d, g, VK_QUEUE_GRAPHICS_BIT)).toBeTruthy();
            expect(family_has(*d, g, VK_QUEUE_COMPUTE_BIT)).toBeTruthy();
            expect(d->queue(g, 0) != VK_NULL_HANDLE).toBeTruthy();
        });

        it("enables timeline semaphores and synchronization2", {
            // The enabled chain is the one handed to FFmpeg; walk it for the 1.2/1.3 structs.
            bool timeline = false;
            bool sync2 = false;
            for (auto* s = static_cast<const VkBaseInStructure*>(d->enabled_features().pNext);
                 s != nullptr; s = s->pNext) {
                if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
                    timeline = reinterpret_cast<const VkPhysicalDeviceVulkan12Features*>(s)
                                   ->timelineSemaphore;
                }
                if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
                    sync2 = reinterpret_cast<const VkPhysicalDeviceVulkan13Features*>(s)
                                ->synchronization2;
                }
            }
            expect(timeline).toBeTruthy();
            expect(sync2).toBeTruthy();
        });

        it("only reports extensions it enabled", {
            for (const char* e : d->enabled_extensions()) {
                expect(d->has_extension(e)).toBeTruthy();
            }
            expect(d->has_extension("VK_NOT_A_REAL_extension")).toBeFalsy();
        });

        it("matches the queue creation flags to internal synchronization", {
            const bool flagged = d->queue_create_flags() != 0;
            expect(flagged == d->internally_synchronized_queues()).toBeTruthy();
        });

        it("locks and unlocks every queue it created", {
            for (const auto& f : d->queue_families()) {
                for (uint32_t i = 0; i < f.count; ++i) {
                    d->lock_queue(f.index, i);
                    d->unlock_queue(f.index, i);
                }
            }
            expect(true).toBeTruthy();
        });
    });

    describe("gpu::enumerate_devices", {
        it("lists the device that was created", {
            Device* d = device();
            auto all = oma::gpu::enumerate_devices();
            if (d == nullptr) {
                expect(true).toBeTruthy();
            } else {
                expect(all.has_value()).toBeTruthy();
                bool found = false;
                for (const auto& info : *all) {
                    found = found || info.name == d->info().name;
                }
                expect(found).toBeTruthy();
            }
        });

        it("rejects a device name that does not exist", {
            auto r = Device::create({.name_contains = std::string("no-such-gpu-xyz")});
            expect(r.has_value()).toBeFalsy();
            expect(static_cast<int>(r.error().code()))
                .toEqual(static_cast<int>(ErrorCode::Unsupported));
        });
    });
}

void release_gpu_test_device() {
    g_device.reset();
}
