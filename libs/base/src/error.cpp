#include "oma/base/error.hpp"

#include <format>

namespace oma {

std::string_view to_string(Category category) noexcept {
    switch (category) {
    case Category::Base:
        return "base";
    case Category::Media:
        return "media";
    case Category::Decode:
        return "decode";
    case Category::Encode:
        return "encode";
    case Category::Gpu:
        return "gpu";
    case Category::Compositor:
        return "compositor";
    case Category::Timeline:
        return "timeline";
    case Category::Project:
        return "project";
    case Category::Importer:
        return "importer";
    case Category::Audio:
        return "audio";
    case Category::Cache:
        return "cache";
    case Category::Playback:
        return "playback";
    case Category::Ui:
        return "ui";
    }
    return "unknown";
}

std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidArgument:
        return "invalid argument";
    case ErrorCode::Overflow:
        return "overflow";
    case ErrorCode::OutOfRange:
        return "out of range";
    case ErrorCode::Cancelled:
        return "cancelled";
    case ErrorCode::Unsupported:
        return "unsupported";
    case ErrorCode::IoError:
        return "I/O error";
    case ErrorCode::InvalidData:
        return "invalid data";
    case ErrorCode::Internal:
        return "internal error";
    }
    return "unknown error";
}

Error::Error(ErrorCode code, Category category, std::string message, std::string context)
    : code_(code), category_(category), message_(std::move(message)), context_(std::move(context)) {
}

std::string Error::summary() const {
    if (context_.empty()) {
        return std::format("[{}] {}: {}", to_string(category_), to_string(code_), message_);
    }
    return std::format("[{}] {}: {} ({})", to_string(category_), to_string(code_), message_,
                       context_);
}

} // namespace oma
