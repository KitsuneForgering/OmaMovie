#include "thumbnail.hpp"

#include <QBuffer>
#include <QFile>

#include <chrono>
#include <filesystem>
#include <format>
#include <span>

namespace tl = oma::timeline;

std::string media_identity(const QString& path, const oma::project::Fingerprint& fingerprint) {
    std::error_code ec;
    const std::filesystem::path file(path.toStdString());
    const auto time = std::filesystem::last_write_time(file, ec);
    const auto ns =
        ec ? 0
           : std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count();
    return std::format("{} {} {} {:016x}", file.string(), fingerprint.size, ns, fingerprint.hash);
}

bool write_thumbnail(FrameSource& frames, const oma::DiskCache& cache, const std::string& file,
                     const std::string& identity, const tl::MediaInfo& media, const QString& out) {
    // Auto keeps the key thumbnails had before readings existed, so those stay valid.
    std::string key = "thumbnail/v1 256x144 " + identity;
    if (media.color_matrix != tl::MediaColorMatrix::Auto ||
        media.color_range != tl::MediaColorRange::Auto)
        key += std::format(" read {} {}", static_cast<int>(media.color_matrix),
                           static_cast<int>(media.color_range));
    QFile png_file(out);
    if (const auto cached = cache.get(key)) {
        return png_file.open(QIODevice::WriteOnly) &&
               png_file.write(reinterpret_cast<const char*>(cached->data()),
                              static_cast<qint64>(cached->size())) ==
                   static_cast<qint64>(cached->size());
    }
    auto first = frames.image_at(file, media.start, &media);
    if (!first)
        return false;
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!first->scaled(256, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation)
             .save(&buffer, "PNG") ||
        !png_file.open(QIODevice::WriteOnly) || png_file.write(png) != png.size())
        return false;
    (void)cache.put(key, std::span(reinterpret_cast<const std::uint8_t*>(png.constData()),
                                   static_cast<std::size_t>(png.size())));
    return true;
}
