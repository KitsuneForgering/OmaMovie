#include "autosave.hpp"

#include <QFileInfo>

#include <QCryptographicHash>

#include <QFile>
#include <QStandardPaths>

#include <algorithm>
#include <system_error>

namespace autosave {

std::filesystem::path dir() {
    const QString state = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    return state.isEmpty() ? std::filesystem::path{} : std::filesystem::path(state.toStdString()) / "omamovie" / "autosave";
}

std::filesystem::path origin_of(const std::filesystem::path& file) {
    auto sidecar = file;
    sidecar += ".origin";
    return sidecar;
}

std::vector<Recovered> list(const std::filesystem::path& folder) {
    std::vector<Recovered> out;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(folder, ec)) {
        if (e.path().extension() != ".omamovie" || !e.is_regular_file(ec)) continue;
        Recovered r{.file = e.path(), .origin = {}, .when = {}};
        if (QFile sidecar(QString::fromStdString(origin_of(e.path()).string())); sidecar.open(QIODevice::ReadOnly)) {
            r.origin = QString::fromUtf8(sidecar.read(4096)).trimmed();
        }
        const auto time = e.last_write_time(ec);
        r.when = QDateTime::fromStdTimePoint(std::chrono::time_point_cast<std::chrono::milliseconds>(
            std::chrono::clock_cast<std::chrono::system_clock>(time)));
        out.push_back(std::move(r));
    }
    std::ranges::sort(out, [](const Recovered& a, const Recovered& b) { return a.when > b.when; });
    return out;
}

void remove(const std::filesystem::path& file) {
    std::error_code ec;
    std::filesystem::remove(file, ec);
    std::filesystem::remove(origin_of(file), ec);
}

} // namespace autosave

namespace versions {

std::filesystem::path dir_for(const QString& project) {
    const auto base = autosave::dir();
    if (base.empty()) return {};
    const QFileInfo info(project);
    const QByteArray hash = QCryptographicHash::hash(info.absoluteFilePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
    return base.parent_path() / "versions" / (info.completeBaseName().toStdString() + "-" + hash.toStdString());
}

bool archive(const QString& project) {
    std::error_code ec;
    const std::filesystem::path source(project.toStdString());
    const auto folder = dir_for(project);
    if (folder.empty() || !std::filesystem::is_regular_file(source, ec)) return true; // nothing to keep yet
    std::filesystem::create_directories(folder, ec);
    const auto copy = folder / (QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")).toStdString() + ".omamovie");
    if (!std::filesystem::copy_file(source, copy, std::filesystem::copy_options::overwrite_existing, ec)) return false;
    if (QFile sidecar(QString::fromStdString(autosave::origin_of(copy).string())); sidecar.open(QIODevice::WriteOnly)) {
        sidecar.write(QFileInfo(project).absoluteFilePath().toUtf8());
    }
    const auto all = autosave::list(folder); // newest first
    for (std::size_t i = kKeep; i < all.size(); ++i) autosave::remove(all[i].file);
    return true;
}

std::vector<autosave::Recovered> list(const QString& project) {
    const auto folder = dir_for(project);
    return folder.empty() ? std::vector<autosave::Recovered>{} : autosave::list(folder);
}

} // namespace versions
