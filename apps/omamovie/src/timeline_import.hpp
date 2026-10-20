#pragma once

// An OpenTimelineIO file as a native project document (ADR-0008): the importer, then the converter
// with media probed as the library import does. No UI; runs on a job worker.

#include "oma/media/format.hpp"
#include "oma/project/document.hpp"

#include <QString>
#include <QStringList>

#include <filesystem>
#include <optional>

// The document, with the import report's notes appended to `issues`; none and `why` on failure.
// `edl_rate` is the frame rate an EDL's timecodes count in. An imported speed that the mixer at
// `mixer_rate` plays without sound gets a note too, as the Speed action gives one.
[[nodiscard]] std::optional<oma::project::Document>
import_timeline(const std::filesystem::path& path, const oma::media::MediaImporter& importer,
                QStringList& issues, QString& why, oma::Rational edl_rate,
                oma::SampleRate mixer_rate);

// "25" or "30000/1001" as a positive rate; none otherwise.
[[nodiscard]] std::optional<oma::Rational> edl_rate(const QString& text);
