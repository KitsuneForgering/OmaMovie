#pragma once

// Shared helpers for the libs/media tests. Include before oma_test.hpp.
//
// Test bodies live in plain functions called from it() blocks: a `return` inside an it() block
// would leave the whole suite (Cest blocks are not functions).

#include "oma/base/error.hpp"

#include <filesystem>

namespace oma::gpu {
class Device;
}

// Path of a generated fixture: $OMA_FIXTURES/<name>, default tests/fixtures/generated/<name>.
std::filesystem::path fixture(const char* name);

// Whether the fixture exists; prints why the test is skipped when it does not (the generator
// skips codecs the local FFmpeg cannot encode).
bool have_fixture(const char* name);

// The shared Vulkan device, or nullptr (with a printed note) on machines without a driver.
const oma::gpu::Device* media_test_device();
void release_media_test_device();

int code_of(const oma::Error& e);
