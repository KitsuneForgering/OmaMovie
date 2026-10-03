#pragma once

#include "oma/base/category.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

// Structured logging facade (CLAUDE.md §19). Independent of Qt: the app installs a
// sink that forwards to QLoggingCategory. Never log from the real-time audio thread.

namespace oma {

enum class LogLevel : std::uint8_t {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Off,
};

[[nodiscard]] std::string_view to_string(LogLevel level) noexcept;

struct LogRecord {
    LogLevel level;
    Category category;
    std::string_view message;
    std::source_location location;
    std::chrono::system_clock::time_point time;
};

using LogSink = std::function<void(const LogRecord&)>;

// Process-wide configuration. Thread-safe. Passing an empty sink restores the
// default sink, which writes one line per record to stderr.
// The sink runs under the logger's internal lock: it must not log, must not call
// set_log_sink(), and must not hold a lock that another thread keeps while logging.
void set_log_sink(LogSink sink);
void set_log_level(Category category, LogLevel min_level) noexcept;
void set_log_level_all(LogLevel min_level) noexcept;

// Cheap check (one relaxed atomic load) done before any formatting.
[[nodiscard]] bool log_enabled(Category category, LogLevel level) noexcept;

void log_message(LogLevel level, Category category, std::string_view message,
                 std::source_location location = std::source_location::current());

// Format string that also captures the call site.
template <typename... Args>
struct LogFormat {
    template <typename S>
        requires std::convertible_to<const S&, std::string_view>
    consteval LogFormat(const S& fmt, std::source_location loc = std::source_location::current())
        : format(fmt), location(loc) {}

    std::format_string<Args...> format;
    std::source_location location;
};

template <typename... Args>
void log(LogLevel level, Category category, LogFormat<std::type_identity_t<Args>...> fmt,
         Args&&... args) {
    if (!log_enabled(category, level)) {
        return;
    }
    log_message(level, category, std::format(fmt.format, std::forward<Args>(args)...),
                fmt.location);
}

template <typename... Args>
void log_debug(Category category, LogFormat<std::type_identity_t<Args>...> fmt, Args&&... args) {
    log<Args...>(LogLevel::Debug, category, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_info(Category category, LogFormat<std::type_identity_t<Args>...> fmt, Args&&... args) {
    log<Args...>(LogLevel::Info, category, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_warn(Category category, LogFormat<std::type_identity_t<Args>...> fmt, Args&&... args) {
    log<Args...>(LogLevel::Warn, category, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void log_error(Category category, LogFormat<std::type_identity_t<Args>...> fmt, Args&&... args) {
    log<Args...>(LogLevel::Error, category, fmt, std::forward<Args>(args)...);
}

} // namespace oma
