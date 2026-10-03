#include "waveform_store.hpp"

#include "oma/base/log.hpp"

#include <QMetaObject>

#include <filesystem>
#include <utility>

WaveformStore::~WaveformStore() {
    pool_.shutdown();
}

void WaveformStore::request(std::uint64_t media, const std::string& path) {
    if (waveforms_.contains(media) || pending_.contains(media)) return;
    pending_[media] = true;
    std::erase_if(jobs_, [](const oma::JobHandle& h) {
        const auto s = h.state();
        return s != oma::JobState::Pending && s != oma::JobState::Running;
    });
    const unsigned generation = generation_;
    jobs_.push_back(pool_.submit("waveform", [this, media, path, generation](oma::JobContext& job) {
        auto computed = oma::playback::compute_waveform(std::filesystem::path(path), job);
        if (!computed) {
            if (computed.error().code() != oma::ErrorCode::Cancelled) {
                // Degrade visibly in the log, never silently (CLAUDE.md §19): the clip just has no
                // waveform drawn.
                oma::log_warn(oma::Category::Audio, "no waveform for {}: {}", path, computed.error().summary());
            }
            return oma::Result<void>{};
        }
        auto shared = std::make_shared<const oma::playback::Waveform>(std::move(*computed));
        QMetaObject::invokeMethod(this, [this, media, generation, shared = std::move(shared)]() mutable {
            if (generation != generation_) return;
            pending_.erase(media);
            waveforms_[media] = std::move(shared);
            emit ready(static_cast<double>(media));
        }, Qt::QueuedConnection);
        return oma::Result<void>{};
    }));
}

void WaveformStore::clear() {
    ++generation_;
    for (oma::JobHandle& h : jobs_) h.cancel();
    jobs_.clear();
    pending_.clear();
    waveforms_.clear();
}

std::shared_ptr<const oma::playback::Waveform> WaveformStore::get(std::uint64_t media) const {
    const auto it = waveforms_.find(media);
    return it == waveforms_.end() ? nullptr : it->second;
}
