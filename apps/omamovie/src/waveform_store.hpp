#pragma once

#include "oma/base/jobs.hpp"
#include "oma/playback/waveform.hpp"

#include <QObject>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Waveforms of the library's media (ui-design §7.2), computed in the background and kept for the
// session. Not a disk cache yet: the cache strategy is ADR-0009's (CLAUDE.md §15).
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

    // Starts computing the waveform of `media` unless it exists or is being computed.
    void request(std::uint64_t media, const std::string& path);
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
    oma::JobPool pool_{1}; // destroyed first: no job outlives the store
};
