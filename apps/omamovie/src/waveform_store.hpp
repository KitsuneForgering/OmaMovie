#pragma once

#include "oma/base/disk_cache.hpp"
#include "oma/base/jobs.hpp"
#include "oma/playback/waveform.hpp"

#include <QObject>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Waveforms of the library's media (ui-design §7.2), computed in the background, kept for the
// session and in the disk cache (ADR-0009) under the media's identity, so reopening a project
// does not decode its sound again.
//
// Threading: lives on the UI thread. Each waveform is computed by a job on this store's own pool
// and arrives back through a queued invocation; it is immutable from then on, so the timeline's
// items may read it from Qt's render thread while the UI thread is blocked in sync.
class WaveformStore final : public QObject {
    Q_OBJECT
public:
    explicit WaveformStore(QObject* parent = nullptr) : QObject(parent) {}
    ~WaveformStore() override;
    WaveformStore(const WaveformStore&) = delete;
    WaveformStore& operator=(const WaveformStore&) = delete;
    WaveformStore(WaveformStore&&) = delete;
    WaveformStore& operator=(WaveformStore&&) = delete;

    // The disk cache to read first and fill (may be null; must outlive the store's jobs).
    void set_cache(const oma::DiskCache* cache) { cache_ = cache; }
    // Starts computing the waveform of `media` unless it exists or is being computed.
    // `identity` names the file's content (path, size, time, fingerprint) for the disk cache.
    void request(std::uint64_t media, const std::string& path, const std::string& identity);
    // Forgets every waveform and cancels the jobs in flight (a new project).
    void clear();

    [[nodiscard]] std::shared_ptr<const oma::playback::Waveform> get(std::uint64_t media) const;

signals:
    void ready(double media);

private:
    std::unordered_map<std::uint64_t, std::shared_ptr<const oma::playback::Waveform>> waveforms_;
    std::vector<oma::JobHandle> jobs_;
    std::unordered_map<std::uint64_t, bool> pending_;
    unsigned generation_ = 0;
    const oma::DiskCache* cache_ = nullptr;
    oma::JobPool pool_{1}; // destroyed first: no job outlives the store
};
