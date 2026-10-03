#include "media_test.hpp"

#include "oma/base/log.hpp"
#include "oma/gpu/device.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include "oma_test.hpp"

void run_probe_tests();
void run_format_tests();
void run_video_decoder_tests();
void run_gpu_decode_tests();
void run_audio_decoder_tests();

namespace {

std::unique_ptr<oma::gpu::Device> g_device;
bool g_attempted = false;

} // namespace

std::filesystem::path fixture(const char* name) {
    const char* dir = std::getenv("OMA_FIXTURES");
    return std::filesystem::path(dir != nullptr ? dir : "tests/fixtures/generated") / name;
}

bool have_fixture(const char* name) {
    if (std::filesystem::exists(fixture(name))) {
        return true;
    }
    std::printf("    (skipped: fixture %s was not generated)\n", name);
    return false;
}

const oma::gpu::Device* media_test_device() {
    if (!g_attempted) {
        g_attempted = true;
        auto d = oma::gpu::Device::create();
        if (d) {
            g_device = std::move(*d);
        } else {
            std::printf("  (no Vulkan device, GPU cases skip: %s)\n", d.error().summary().c_str());
        }
    }
    return g_device.get();
}

void release_media_test_device() {
    g_device.reset();
}

int code_of(const oma::Error& e) {
    return static_cast<int>(e.code());
}

int main(int argc, char* argv[]) {
    // Expected warnings (damaged fixtures, downgrades) and FFmpeg's own complaints about them
    // stay out of the test output.
    oma::set_log_level_all(oma::LogLevel::Error);
    oma::set_log_level(oma::Category::Media, oma::LogLevel::Off);
    cest_init(argc, argv);
    run_probe_tests();
    run_format_tests();
    run_video_decoder_tests();
    run_gpu_decode_tests();
    run_audio_decoder_tests();
    release_media_test_device();
    return cest_result();
}
