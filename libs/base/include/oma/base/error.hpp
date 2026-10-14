#pragma once

#include "oma/base/category.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace oma {

enum class ErrorCode : std::uint8_t {
    InvalidArgument,
    Overflow,
    OutOfRange,
    Cancelled,
    Unsupported,
    IoError,
    InvalidData,
    Internal,
    DeviceLost, // the GPU device stopped working; nothing more can run on it
};

[[nodiscard]] std::string_view to_string(ErrorCode code) noexcept;

// Error crossing a subsystem boundary. Exceptions never cross library boundaries
// (CLAUDE.md §19); failures travel as Result<T> = std::expected<T, Error>.
class Error {
public:
    Error(ErrorCode code, Category category, std::string message, std::string context = {});

    [[nodiscard]] ErrorCode code() const noexcept;
    [[nodiscard]] Category category() const noexcept;
    [[nodiscard]] const std::string& message() const noexcept;
    [[nodiscard]] const std::string& context() const noexcept;

    // Human-readable form: "[category] code: message (context)".
    // (Not named describe(): that name is a Cest macro in test files.)
    [[nodiscard]] std::string summary() const;

private:
    ErrorCode code_;
    Category category_;
    std::string message_;
    std::string context_;
};

template <typename T>
using Result = std::expected<T, Error>;

[[nodiscard]] std::unexpected<Error>
make_error(ErrorCode code, Category category, std::string message, std::string context = {});

} // namespace oma
