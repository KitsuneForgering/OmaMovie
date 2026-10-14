// The recording workflow on a real Omarchy recording (M10): OMA_REAL_RECORDING=file runs it; the
// file is the user's and never enters the repository. Assumes the recorder's 60 fps CFR intent,
// so the 0.25 s cut drops 15 frames.
#include "decoded_frames.hpp"
#include "real_recording.hpp"
#include "recording_edit.hpp"
#include "sound_sync.hpp"

#include <algorithm>

#include "oma_test.hpp"

namespace {

bool real_recording_round_trips() {
    const QString path = qEnvironmentVariable("OMA_REAL_RECORDING");
    QTemporaryDir dir;
    const QString movie = export_recording_edit(dir, path, 600000);
    const int frames = decoded_frames(path);
    if (movie.isEmpty() || decoded_frames(movie) != frames - 15)
        return false;
    const QByteArray source = gray_frames(path);
    const QByteArray out = gray_frames(movie);
    for (int i = 0; i < frames - 15; i += 15)
        if (!shows_frame(source, out, i, 15 + i))
            return false;
    const auto recorded = mixed_sound(path);
    const auto exported = mixed_sound(movie);
    if (recorded.empty())
        return true; // recorded without sound: nothing to keep in sync
    // Every half second of the export; quiet windows are skipped (a recording is often silence
    // with a few sounds), but sound in the recording that no window compared fails.
    int compared = 0;
    for (std::size_t at = 24000; at + 72000 < exported.size(); at += 24000) {
        if (const auto lag = best_lag(recorded, exported, at, 12000)) {
            if (std::abs(*lag) > 48)
                return false; // more than 1 ms off the recording
            ++compared;
        }
    }
    const bool sound = std::any_of(recorded.begin() + 48000, recorded.end() - 48000,
                                   [](float v) { return std::abs(v) > 0.01F; });
    return compared > 0 || !sound;
}

} // namespace

void run_real_recording_tests() {
    if (!qEnvironmentVariableIsSet("OMA_REAL_RECORDING"))
        return;
    describe("Real recording", {
        it("keeps a real recording's frames and sound sync through the workflow",
           { expect(real_recording_round_trips()).toBeTruthy(); });
    });
}
