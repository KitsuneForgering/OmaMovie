#include "export_recovery.hpp"

#include <fstream>
#include <system_error>

namespace export_recovery {
namespace fs = std::filesystem;
namespace {
oma::Result<void> io_error(const fs::path& path, const std::error_code& ec) {
    return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                           "cannot recover export: " + ec.message(), path.string());
}

oma::Result<void> restore_one(const fs::path& current, const fs::path& staged,
                              const fs::path& previous, bool had_previous) {
    std::error_code ec;
    const bool backup_exists = fs::exists(previous, ec);
    if (ec)
        return io_error(previous, ec);
    const bool staged_exists = fs::exists(staged, ec);
    if (ec)
        return io_error(staged, ec);
    if (had_previous && backup_exists) {
        fs::copy_file(previous, current, fs::copy_options::overwrite_existing, ec);
        if (ec)
            return io_error(current, ec);
    } else if (had_previous && !staged_exists) {
        return oma::make_error(oma::ErrorCode::InvalidData, oma::Category::Encode,
                               "previous export file is unavailable", previous.string());
    } else if (had_previous && !fs::exists(current, ec)) {
        return oma::make_error(oma::ErrorCode::InvalidData, oma::Category::Encode,
                               "previous output is unavailable", current.string());
    } else if (!had_previous && !staged_exists) {
        fs::remove(current, ec);
        if (ec)
            return io_error(current, ec);
    }
    return {};
}
} // namespace

State state(const Record& record) {
    if (fs::exists(record.stage / ".restored"))
        return State::Interrupted;
    const bool publishing = fs::exists(record.stage / ".publishing");
    if (!publishing)
        return State::Staged;
    const bool movie_done =
        !fs::exists(record.stage / record.movie.filename()) && fs::exists(record.movie);
    const bool captions_done =
        !record.sidecar ||
        (!fs::exists(record.stage / record.sidecar->filename()) && fs::exists(*record.sidecar));
    return movie_done && captions_done ? State::Completed : State::Interrupted;
}

oma::Result<void> restore_previous(const Record& record) {
    if (fs::exists(record.stage / ".restored")) {
        std::error_code ec;
        fs::remove_all(record.stage, ec);
        if (ec)
            return io_error(record.stage, ec);
        forget(record.stage);
        return {};
    }
    if (state(record) != State::Staged) {
        auto movie = restore_one(record.movie, record.stage / record.movie.filename(),
                                 record.stage / "previous-movie", record.had_movie);
        if (!movie)
            return movie;
        if (record.sidecar) {
            auto captions = restore_one(*record.sidecar, record.stage / record.sidecar->filename(),
                                        record.stage / "previous-captions", record.had_sidecar);
            if (!captions)
                return captions;
        }
    }
    {
        std::ofstream marker(record.stage / ".restored", std::ios::binary);
        marker << "1\n";
        marker.close();
        if (!marker)
            return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                                   "cannot record export recovery", record.stage.string());
    }
    std::error_code ec;
    fs::remove_all(record.stage, ec);
    if (ec)
        return io_error(record.stage, ec);
    forget(record.stage);
    return {};
}

oma::Result<void> keep_current(const Record& record) {
    if (state(record) != State::Completed)
        return oma::make_error(oma::ErrorCode::InvalidData, oma::Category::Encode,
                               "export is not complete", record.movie.string());
    std::error_code ec;
    fs::remove_all(record.stage, ec);
    if (ec)
        return io_error(record.stage, ec);
    forget(record.stage);
    return {};
}
} // namespace export_recovery
