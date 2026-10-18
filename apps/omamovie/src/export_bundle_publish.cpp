#include "export_bundle_publish.hpp"

#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

oma::Result<void> publish_export_bundle(const fs::path& stage, const fs::path& movie,
                                        const fs::path& staged_movie,
                                        const std::optional<fs::path>& sidecar,
                                        const std::optional<fs::path>& staged_sidecar,
                                        const PublishFault& fault) {
    const fs::path old_movie = stage / "previous-movie";
    const fs::path old_sidecar = stage / "previous-captions";
    {
        std::ofstream marker(stage / ".publishing", std::ios::binary);
        marker << "1\n";
        marker.close();
        if (!marker)
            return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                                   "cannot record export publication", stage.string());
    }
    std::error_code ec;
    const bool had_movie = fs::exists(movie, ec);
    if (ec)
        return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                               "cannot inspect output", movie.string());
    const bool had_sidecar = sidecar && fs::exists(*sidecar, ec);
    if (ec)
        return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                               "cannot inspect captions", sidecar->string());
    if (had_movie && fault)
        ec = fault(PublishStep::BackupMovie);
    if (had_movie && !ec)
        fs::rename(movie, old_movie, ec);
    const bool backed_movie = had_movie && !ec;
    if (!ec && had_sidecar && fault)
        ec = fault(PublishStep::BackupSidecar);
    if (!ec && had_sidecar)
        fs::rename(*sidecar, old_sidecar, ec);
    const bool backed_sidecar = had_sidecar && !ec;
    if (!ec && fault)
        ec = fault(PublishStep::PublishMovie);
    if (!ec)
        fs::rename(staged_movie, movie, ec);
    const bool published_movie = !ec;
    if (!ec && sidecar && staged_sidecar && fault)
        ec = fault(PublishStep::PublishSidecar);
    if (!ec && sidecar && staged_sidecar)
        fs::rename(*staged_sidecar, *sidecar, ec);
    if (!ec && fault)
        ec = fault(PublishStep::Completed);
    if (!ec) {
        std::error_code cleanup;
        fs::remove_all(stage, cleanup); // the new pair is complete; old files are no longer needed
        return {};
    }

    const std::string problem = ec.message();
    std::error_code rollback;
    if (published_movie)
        fs::remove(movie, rollback);
    if (backed_movie && !rollback)
        fs::rename(old_movie, movie, rollback);
    if (backed_sidecar) {
        std::error_code side_error;
        fs::rename(old_sidecar, *sidecar, side_error);
        if (side_error)
            rollback = side_error;
    }
    const std::string detail = rollback ? "rollback failed; backups are in " + stage.string()
                                        : "previous outputs restored";
    return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                           "cannot publish export: " + problem + "; " + detail);
}
