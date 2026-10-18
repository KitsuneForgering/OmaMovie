#include "oma/gpu/commands.hpp"

#include "oma_test.hpp"

// One VkResult mapping for libs/gpu and libs/compositor (code review 2026-10-14): memory
// exhaustion used to read as Unsupported in one place and Internal in another.

namespace {
int code(VkResult r) {
    return static_cast<int>(oma::gpu::error_code(r));
}
} // namespace

void run_error_code_tests() {
    describe("gpu::error_code", {
        it("tells a lost device and exhausted memory from other failures", {
            expect(code(VK_ERROR_DEVICE_LOST)).toBe(static_cast<int>(oma::ErrorCode::DeviceLost));
            expect(code(VK_ERROR_OUT_OF_DEVICE_MEMORY))
                .toBe(static_cast<int>(oma::ErrorCode::OutOfMemory));
            expect(code(VK_ERROR_OUT_OF_HOST_MEMORY))
                .toBe(static_cast<int>(oma::ErrorCode::OutOfMemory));
            expect(code(VK_ERROR_INITIALIZATION_FAILED))
                .toBe(static_cast<int>(oma::ErrorCode::Internal));
        });
    });
}
