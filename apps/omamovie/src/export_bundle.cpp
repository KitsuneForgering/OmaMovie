#include "export_bundle.hpp"
#include "export_bundle_publish.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

oma::Result<void>
export_movie_bundle(ExportRequest request, std::optional<std::string> captions,
                    const std::function<bool(std::int64_t, std::int64_t)>& progress,
                    ExportStats* stats) {
    const fs::path movie = request.file;
    const fs::path sidecar = movie.parent_path() / (movie.stem().string() + ".srt");
    std::string pattern = (movie.parent_path() / ".omamovie-export-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr)
        return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                               "cannot stage export", movie.string());
    const fs::path stage(pattern);
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code ec;
            const bool movie_backup = fs::exists(path / "previous-movie", ec);
            if (ec)
                return;
            const bool caption_backup = fs::exists(path / "previous-captions", ec);
            if (!ec && !movie_backup && !caption_backup)
                fs::remove_all(path, ec);
        }
    } cleanup{stage};
    const fs::path staged_movie = stage / movie.filename();
    const fs::path staged_sidecar = stage / sidecar.filename();
    if (captions) {
        std::ofstream out(staged_sidecar, std::ios::binary);
        out.write(captions->data(), static_cast<std::streamsize>(captions->size()));
        out.close();
        if (!out)
            return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                                   "cannot stage captions", sidecar.string());
    }
    request.file = staged_movie;
    if (auto rendered = export_timeline(request, progress, stats); !rendered)
        return rendered;
    if (!captions) {
        std::error_code ec;
        const bool existing_sidecar = fs::exists(sidecar, ec);
        if (ec || existing_sidecar)
            return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                                   "captions appeared beside the output during export",
                                   sidecar.string());
    }
    return publish_export_bundle(stage, movie, staged_movie,
                                 captions ? std::optional<fs::path>(sidecar) : std::nullopt,
                                 captions ? std::optional<fs::path>(staged_sidecar) : std::nullopt);
}
