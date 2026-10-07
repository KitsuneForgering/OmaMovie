#include "app_settings.hpp"

#include <QDir>
#include <QSettings>
#include <QStringList>

#include <utility>

namespace {

const QStringList kQualities{QStringLiteral("auto"), QStringLiteral("full"), QStringLiteral("half"),
                             QStringLiteral("quarter")};
const QStringList kCacheLimits{QStringLiteral("512"), QStringLiteral("256"), QStringLiteral("1024"),
                               QStringLiteral("4096")}; // the default first
const QStringList kExportEncoders{QStringLiteral("auto"), QStringLiteral("hardware"), QStringLiteral("software")};
const QStringList kDecodePaths{QStringLiteral("auto"), QStringLiteral("hardware"), QStringLiteral("software")};

QString one_of(const QString& value, const QStringList& allowed) {
    return allowed.contains(value) ? value : allowed.front();
}

QString folder_or_default(const QString& value) {
    return value.isEmpty() || !QDir::isAbsolutePath(value) ? QString() : QDir::cleanPath(value);
}

} // namespace

AppSettings::AppSettings(QString settings_file, QObject* parent)
    : QObject(parent), settings_file_(std::move(settings_file)) {
    const QSettings settings(settings_file_, QSettings::IniFormat);
    preview_quality_ = one_of(settings.value("preview/quality").toString(), kQualities);
    decode_path_ = one_of(settings.value("preview/decode").toString(), kDecodePaths);
    recordings_folder_ = folder_or_default(settings.value("recordings/folder").toString());
    cache_limit_ = one_of(settings.value("cache/limit_mb").toString(), kCacheLimits);
    export_encoder_ = one_of(settings.value("export/encoder").toString(), kExportEncoders);
    startup_decode_path_ = decode_path_;
}

void AppSettings::store(const char* key, const QString& value, const QString& fallback) {
    QSettings settings(settings_file_, QSettings::IniFormat);
    // Defaults are not written, so a later default change reaches users who never chose.
    if (value == fallback) {
        settings.remove(QLatin1String(key));
    } else {
        settings.setValue(QLatin1String(key), value);
    }
    emit changed();
}

void AppSettings::setPreviewQuality(const QString& value) {
    const QString valid = one_of(value, kQualities);
    if (valid == preview_quality_) return;
    preview_quality_ = valid;
    store("preview/quality", valid, kQualities.front());
}

void AppSettings::setDecodePath(const QString& value) {
    const QString valid = one_of(value, kDecodePaths);
    if (valid == decode_path_) return;
    decode_path_ = valid;
    store("preview/decode", valid, kDecodePaths.front());
}

void AppSettings::setRecordingsFolder(const QString& value) {
    const QString valid = folder_or_default(value);
    if (valid == recordings_folder_) return;
    recordings_folder_ = valid;
    store("recordings/folder", valid, QString());
}

void AppSettings::setCacheLimit(const QString& value) {
    const QString valid = one_of(value, kCacheLimits);
    if (valid == cache_limit_) return;
    cache_limit_ = valid;
    store("cache/limit_mb", valid, kCacheLimits.front());
}

void AppSettings::setExportEncoder(const QString& value) {
    const QString valid = one_of(value, kExportEncoders);
    if (valid == export_encoder_) return;
    export_encoder_ = valid;
    store("export/encoder", valid, kExportEncoders.front());
}

void AppSettings::setSystem(QVariantMap system) {
    system_ = std::move(system);
    emit systemChanged();
}

void AppSettings::resetAll() {
    setPreviewQuality(kQualities.front());
    setDecodePath(kDecodePaths.front());
    setRecordingsFolder(QString());
    setCacheLimit(kCacheLimits.front());
    setExportEncoder(kExportEncoders.front());
}
