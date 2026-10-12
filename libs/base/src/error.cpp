#include "oma/base/error.hpp"

#include <format>
#include <utility>

namespace oma {

std::unexpected<Error> make_error(ErrorCode code, Category category, std::string message,
                                  std::string context) {
    return std::unexpected<Error>(std::in_place, code, category, std::move(message),
                                  std::move(context));
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

ErrorCode Error::code() const noexcept { return code_; }
Category Error::category() const noexcept { return category_; }
const std::string& Error::message() const noexcept { return message_; }
const std::string& Error::context() const noexcept { return context_; }

} // namespace oma
