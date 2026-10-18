#pragma once

// Whole-file reads and writes for the export tests' old and new outputs.
#include <QByteArray>
#include <QFile>
#include <QString>

inline bool put(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

inline QByteArray get(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
