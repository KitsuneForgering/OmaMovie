#include "oma/base/error.hpp"
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
    case ErrorCode::DeviceLost:
        return "GPU device lost";
    case ErrorCode::OutOfMemory:
        return "out of memory";
    case ErrorCode::Internal:
        return "internal error";
    }
    return "unknown error";
}

} // namespace oma
