#pragma once

// A library thumbnail (ADR-0009): the cached PNG for this content and color reading, else the
// first frame decoded, read as the media item says (ADR-0019), scaled to 256x144 and cached.
// Runs on the job worker that owns `frames`.

#include "frame_source.hpp"

#include "oma/base/disk_cache.hpp"
#include "oma/project/document.hpp"
#include "oma/timeline/model.hpp"

#include <QString>

#include <string>

// The content a cache key stands for (ADR-0009): the file, its size and time, and the
// project fingerprint's partial hash, so an edited or replaced file gets new entries.
[[nodiscard]] std::string media_identity(const QString& path,
                                         const oma::project::Fingerprint& fingerprint);

// Writes the PNG to `out`; false when nothing could be written. `identity` names the content
// (media_identity); the color reading is added to the cache key when it is not Auto.
[[nodiscard]] bool write_thumbnail(FrameSource& frames, const oma::DiskCache& cache,
                                   const std::string& file, const std::string& identity,
                                   const oma::timeline::MediaInfo& media, const QString& out);
