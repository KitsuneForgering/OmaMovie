// libFuzzer target for the media probe (CLAUDE.md §18, M10): any bytes give a handled error or a
// MediaInfo whose stream choices point at real streams and whose durations are not negative,
// never a crash or hang. FFmpeg itself is the system library (not instrumented); this checks
// OmaMovie's conversion of whatever it reports. Inputs go through a memfd, not the disk.

#include "oma/base/log.hpp"
#include "oma/media/probe.hpp"

#include <sys/mman.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// Quiet logging: a warning per input would cost more than the input itself and drown the
// fuzzer's own output; the code under test runs the same.
extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
    oma::set_log_level_all(oma::LogLevel::Off);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const int fd = ::memfd_create("probe", MFD_CLOEXEC);
    if (fd < 0 || ::write(fd, data, size) != static_cast<ssize_t>(size)) {
        if (fd >= 0)
            ::close(fd);
        return 0;
    }
    auto info = oma::media::probe("/proc/self/fd/" + std::to_string(fd));
    ::close(fd);
    if (!info)
        return 0;
    const auto valid = [&](const std::optional<int>& index) {
        return !index || (*index >= 0 && static_cast<std::size_t>(*index) < info->streams.size());
    };
    if (!valid(info->best_video) || !valid(info->best_audio))
        __builtin_trap();
    if (info->duration && info->duration->value() < 0)
        __builtin_trap();
    for (const auto& s : info->streams)
        if (s.duration && s.duration->value() < 0)
            __builtin_trap();
    return 0;
}
