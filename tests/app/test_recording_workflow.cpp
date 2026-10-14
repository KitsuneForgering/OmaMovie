#include "decoded_frames.hpp"
#include "frame_match.hpp"
#include "loudness_match.hpp"
#include "recording_edit.hpp"
#include "tone_frames.hpp"

#include "oma_test.hpp"

namespace {

const QString kRecording = "tests/fixtures/generated/screenrecording_like.mp4";

// Exported frame i shows and plays source frame 15 + i, by an independent decoder.
bool recording_edit_keeps_picture_and_sound() {
    QTemporaryDir dir;
    const QString movie = export_recording_edit(dir, kRecording);
    const QByteArray source = gray_frames(kRecording);
    const QByteArray out = gray_frames(movie);
    if (movie.isEmpty() || out.size() / kFrame != 45 || decoded_frames(movie) != 45)
        return false;
    const auto samples = first_channel(movie);
    for (int i = 0; i < 45; ++i)
        if (closest(source, out, i) != 15 + i || heard_recording_frame(samples, i) != 15 + i)
            return false;
    return true;
}

// A recording without sound (no audio device selected) edits and exports the same way; any
// sound track in the export is silent.
bool silent_recording_exports() {
    QTemporaryDir dir;
    const QString movie =
        export_recording_edit(dir, "tests/fixtures/generated/screenrecording_silent.mp4");
    if (movie.isEmpty() || decoded_frames(movie) != 45)
        return false;
    for (const float s : first_channel(movie))
        if (std::abs(s) > 1e-4F)
            return false;
    return true;
}

} // namespace

void run_recording_workflow_tests() {
    describe("Recording workflow", {
        it("keeps each frame's picture and sound through edit, save, reopen and export",
           { expect(recording_edit_keeps_picture_and_sound()).toBeTruthy(); });
        it("edits and exports a recording without sound",
           { expect(silent_recording_exports()).toBeTruthy(); });
    });
}
