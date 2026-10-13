#include "export_recovery.hpp"

#include "autosave.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace export_recovery {
namespace {
std::filesystem::path folder() {
    const auto autosaves = autosave::dir();
    return autosaves.empty() ? std::filesystem::path{} : autosaves.parent_path() / "exports";
}

std::filesystem::path journal(const std::filesystem::path& stage) {
    return folder() / (stage.filename().string() + ".json");
}

QString string(const std::filesystem::path& path) {
    return QString::fromStdString(path.string());
}
} // namespace

oma::Result<void> remember(const Record& record) {
    if (folder().empty() || !record.stage.is_absolute() || !record.movie.is_absolute() ||
        record.stage.parent_path() != record.movie.parent_path() ||
        (!record.sidecar && record.had_sidecar) ||
        record.stage.filename().string().find(".omamovie-export-") != 0 ||
        (record.sidecar &&
         (record.sidecar->parent_path() != record.movie.parent_path() ||
          *record.sidecar != record.movie.parent_path() / (record.movie.stem().string() + ".srt"))))
        return oma::make_error(oma::ErrorCode::InvalidData, oma::Category::Encode,
                               "invalid export stage", record.stage.string());
    if (!QDir().mkpath(string(folder())))
        return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                               "cannot create export recovery folder", folder().string());
    QSaveFile file(string(journal(record.stage)));
    if (!file.open(QIODevice::WriteOnly))
        return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                               "cannot record export", record.stage.string());
    QJsonObject data{{"stage", string(record.stage)},
                     {"movie", string(record.movie)},
                     {"sidecar", record.sidecar ? string(*record.sidecar) : QString()},
                     {"hadMovie", record.had_movie},
                     {"hadSidecar", record.had_sidecar}};
    const QByteArray bytes = QJsonDocument(data).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size() || !file.commit())
        return oma::make_error(oma::ErrorCode::IoError, oma::Category::Encode,
                               "cannot finish export record", record.stage.string());
    return {};
}

void forget(const std::filesystem::path& stage) {
    QFile::remove(string(journal(stage)));
}

std::vector<Record> pending() {
    std::vector<Record> records;
    if (folder().empty())
        return records;
    const QDir dir(string(folder()));
    for (const QFileInfo& info : dir.entryInfoList({"*.json"}, QDir::Files | QDir::Hidden)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly) || file.size() > 16384)
            continue;
        const QJsonObject data = QJsonDocument::fromJson(file.readAll()).object();
        Record record{.stage = data.value("stage").toString().toStdString(),
                      .movie = data.value("movie").toString().toStdString(),
                      .sidecar = std::nullopt,
                      .had_movie = data.value("hadMovie").toBool(),
                      .had_sidecar = data.value("hadSidecar").toBool()};
        const QString sidecar = data.value("sidecar").toString();
        if (!sidecar.isEmpty())
            record.sidecar = sidecar.toStdString();
        if (record.stage.empty() || record.movie.empty() || !record.stage.is_absolute() ||
            !record.movie.is_absolute() ||
            record.stage.filename().string().find(".omamovie-export-") != 0 ||
            info.fileName().toStdString() != record.stage.filename().string() + ".json" ||
            record.stage.parent_path() != record.movie.parent_path() ||
            (!record.sidecar && record.had_sidecar) ||
            (record.sidecar && (record.sidecar->parent_path() != record.movie.parent_path() ||
                                *record.sidecar != record.movie.parent_path() /
                                                       (record.movie.stem().string() + ".srt"))))
            continue;
        std::error_code inspection;
        const bool stage_exists = std::filesystem::exists(record.stage, inspection);
        if (inspection)
            continue;
        if (!stage_exists) {
            QFile::remove(info.absoluteFilePath());
            continue;
        }
        if (!std::filesystem::is_directory(record.stage, inspection) || inspection)
            continue;
        const auto active = record.stage / ".active";
        const bool has_active = std::filesystem::exists(active, inspection);
        if (inspection)
            continue;
        if (has_active) {
            const int fd = ::open(active.c_str(), O_RDONLY | O_CLOEXEC);
            if (fd < 0)
                continue;
            const bool busy = ::flock(fd, LOCK_EX | LOCK_NB) != 0;
            ::close(fd);
            if (busy)
                continue;
        }
        records.push_back(std::move(record));
    }
    return records;
}
} // namespace export_recovery
