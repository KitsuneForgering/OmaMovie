#include "oma/base/log.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>

namespace oma {

namespace {

// Logging is the one process-wide service in libs/base: every subsystem logs, and the
// app configures the destination once at startup. State is internal and thread-safe.
struct LogState {
    std::array<std::atomic<std::uint8_t>, kCategoryCount> levels{};
    std::mutex sink_mutex;
    LogSink sink;

    LogState() {
        for (auto& level : levels) {
            level.store(static_cast<std::uint8_t>(LogLevel::Info), std::memory_order_relaxed);
        }
    }
};

LogState& state() {
    static LogState s;
    return s;
}

std::string_view file_name(std::string_view path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

void default_sink(const LogRecord& record) {
    const auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(record.time);
    const std::string line = std::format(
        "{:%T} {:5} {:10} {} ({}:{})\n", ms, to_string(record.level), to_string(record.category),
        record.message, file_name(record.location.file_name()), record.location.line());
    std::fputs(line.c_str(), stderr);
}

} // namespace

std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Off:
        return "OFF";
    }
    return "?";
}

void set_log_sink(LogSink sink) {
    auto& s = state();
    const std::scoped_lock lock(s.sink_mutex);
    s.sink = std::move(sink);
}

void set_log_level(Category category, LogLevel min_level) noexcept {
    state().levels[static_cast<std::size_t>(category)].store(static_cast<std::uint8_t>(min_level),
                                                             std::memory_order_relaxed);
}

void set_log_level_all(LogLevel min_level) noexcept {
    for (auto& level : state().levels) {
        level.store(static_cast<std::uint8_t>(min_level), std::memory_order_relaxed);
    }
}

bool log_enabled(Category category, LogLevel level) noexcept {
    if (level == LogLevel::Off) {
        return false;
    }
    const auto min =
        state().levels[static_cast<std::size_t>(category)].load(std::memory_order_relaxed);
    return static_cast<std::uint8_t>(level) >= min;
}

void log_message(LogLevel level, Category category, std::string_view message,
                 std::source_location location) {
    if (!log_enabled(category, level)) {
        return;
    }
    const LogRecord record{.level = level,
                           .category = category,
                           .message = message,
                           .location = location,
                           .time = std::chrono::system_clock::now()};
    auto& s = state();
    // Serializes sink calls so lines from different threads never interleave.
    const std::scoped_lock lock(s.sink_mutex);
    if (s.sink) {
        s.sink(record);
    } else {
        default_sink(record);
    }
}

} // namespace oma
