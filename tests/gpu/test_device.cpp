#include "oma/gpu/commands.hpp"
#include "oma/gpu/device.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

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

// The Qt bridge holds the graphics queue for a frame and the render thread submits inside it;
// any other thread must still wait.
bool queue_lock_is_recursive_and_exclusive(const Device& d) {
    const uint32_t family = d.graphics_family();
    d.lock_queue(family, 0);
    d.lock_queue(family, 0); // re-entry on the same thread
    std::atomic<bool> other_got_it{false};
    std::thread other([&] {
        d.lock_queue(family, 0);
        other_got_it.store(true);
        d.unlock_queue(family, 0);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const bool excluded_while_held = !other_got_it.load();
    d.unlock_queue(family, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool still_excluded = !other_got_it.load(); // one level is still held
    d.unlock_queue(family, 0);
    other.join();
    return excluded_while_held && still_excluded && other_got_it.load();
}

// ADR-0005 admission: open by default; a closed gate holds new work until it opens, lets a
// cancelled waiter go without admission, and closing waits for admitted work to leave.
bool admission_gates_work(const Device& d) {
    const auto never = [] {
        return false;
    };
    if (!d.admit(never)) {
        return false;
    }
    d.leave();

    d.close_admission();
    std::atomic<bool> admitted{false};
    std::thread waiter([&] {
        if (d.admit(never)) {
            admitted.store(true);
            d.leave();
        }
    });
    std::atomic<bool> cancel{false};
    std::atomic<int> cancelled_result{-1};
    std::thread cancelled(
        [&] { cancelled_result.store(d.admit([&] { return cancel.load(); }) ? 1 : 0); });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const bool held = !admitted.load();
    cancel.store(true);
    cancelled.join();
    d.open_admission();
    waiter.join();
    const bool waiter_ran = admitted.load();

    // Closing waits for admitted work.
    std::atomic<bool> inside{false};
    std::atomic<bool> left{false};
    std::thread worker([&] {
        if (d.admit(never)) {
            inside.store(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            left.store(true);
            d.leave();
        }
    });
    while (!inside.load()) {
        std::this_thread::yield();
    }
    d.close_admission();
    const bool waited = left.load();
    d.open_admission();
    worker.join();
    return held && cancelled_result.load() == 0 && waiter_ran && waited;
}

bool family_has(const Device& d, uint32_t family, VkQueueFlags flag) {
    for (const auto& f : d.queue_families()) {
        if (f.index == family) {
            return (f.flags & flag) != 0;
        }
    }
    return false;
}

// Device loss is final: a lost device refuses admission (releasing a waiter held by a closed
// gate) and new submissions, instead of letting producers retry on it.
bool loss_stops_work() {
    auto fresh = Device::create(); // its own device: the shared one must stay usable
    if (!fresh) {
        return false;
    }
    const Device& d = **fresh;
    const auto never = [] {
        return false;
    };
    auto runner = oma::gpu::CommandRunner::create(d, d.graphics_family());
    if (!runner || !runner->run([](VkCommandBuffer) {}, {}, {})) {
        return false;
    }
    d.close_admission();
    std::atomic<int> waiter{-1};
    std::thread t([&] { waiter.store(d.admit(never) ? 1 : 0); });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const bool held = waiter.load() == -1;
    d.mark_lost();
    t.join();
    d.open_admission();
    const auto refused = runner->run([](VkCommandBuffer) {}, {}, {});
    return held && waiter.load() == 0 && d.lost() && !d.admit(never) && !refused &&
           refused.error().code() == oma::ErrorCode::DeviceLost;
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
            std::printf("    device: %s (%s), %s\n", d->info().name.c_str(),
                        d->info().driver_info.c_str(), d->info().render_node.c_str());
        });

        it("reports its advertised interop and video prerequisites", {
            const auto& info = d->info();
            expect(info.dma_buf_import ==
                   d->has_extension(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME))
                .toBeTruthy();
            expect(info.drm_modifiers ==
                   d->has_extension(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME))
                .toBeTruthy();
            expect(info.external_semaphore_fd ==
                   d->has_extension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME))
                .toBeTruthy();
            expect(info.video_decode_queue == d->supports_video_decode()).toBeTruthy();
            expect(info.video_decode_queue || info.advertised_video_codecs == 0).toBeTruthy();
            std::printf("    interop: DMA-BUF %d, DRM modifiers %d, semaphore fd %d; "
                        "video decode queue %d, codec flags 0x%x\n",
                        info.dma_buf_import, info.drm_modifiers, info.external_semaphore_fd,
                        info.video_decode_queue, info.advertised_video_codecs);
        });

        it("names a render node that exists, if it reports one", {
            // VA-API opens this node so decode runs on the compositing GPU (hybrid laptops).
            const std::string& node = d->info().render_node;
            expect(node.empty() || std::filesystem::exists(node)).toBeTruthy();
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

        it("admits work outside Qt's swapchain changes (ADR-0005)",
           { expect(admission_gates_work(*d)).toBeTruthy(); });

        it("stops admitting and submitting once the device is lost",
           { expect(loss_stops_work()).toBeTruthy(); });

        it("can create zero-flag queues for consumers using vkGetDeviceQueue", {
            auto compatible = Device::create({.internally_synchronized_queues = false});
            expect(compatible.has_value()).toBeTruthy();
            if (!compatible) {
                return;
            }
            const auto& c = **compatible;
            expect(c.internally_synchronized_queues()).toBeFalsy();
            expect(c.queue_create_flags()).toEqual(VkDeviceQueueCreateFlags{0});
            // FFmpeg and libplacebo request flagged queues whenever this extension is enabled.
            expect(c.has_extension("VK_KHR_internally_synchronized_queues")).toBeFalsy();
            // Headless: no surface on the instance, so no swapchain on the device.
            expect(c.has_extension("VK_KHR_swapchain")).toBeFalsy();
            VkQueue retrieved = VK_NULL_HANDLE;
            vkGetDeviceQueue(c.device(), c.graphics_family(), 0, &retrieved);
            expect(retrieved != VK_NULL_HANDLE).toBeTruthy();
            expect(retrieved == c.queue(c.graphics_family(), 0)).toBeTruthy();
            c.lock_queue(c.graphics_family(), 0);
            c.unlock_queue(c.graphics_family(), 0);
            expect(queue_lock_is_recursive_and_exclusive(c)).toBeTruthy();
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
