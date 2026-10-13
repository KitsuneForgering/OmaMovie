#include "session.hpp"
#include "wait_for.hpp"

#include <QFileInfo>

#include <cmath>

#include "oma_test.hpp"

namespace {
// Applying Noir to the selected clip adds its looks and its sharpening (shown as the drawer's
// sharpness slider, not in the stack list) in one undoable edit.
bool applies_a_preset_in_one_edit() {
    Session s;
    s.setSilent(true);
    s.open(QFileInfo("tests/fixtures/generated/h264_30fps_aac.mp4").absoluteFilePath());
    if (!wait_for([&] { return s.clips().size() == 1 && !s.importsPending(); }))
        return false;
    s.selectClip(s.clips().front().toMap().value("id").toDouble());
    const auto effects = [&] {
        return s.info().value("effects").toList().size();
    };
    const bool listed = s.effectPresets().size() >= 4 && effects() == 0;
    s.applyEffectPreset(QStringLiteral("noir"));
    const bool applied =
        effects() == 2 && std::abs(s.info().value("sharpness").toDouble() - 0.2) < 1e-9;
    s.undo();
    return listed && applied && effects() == 0;
}
} // namespace

void run_effect_preset_app_tests() {
    describe("Effect presets in the app", {
        it("applies a preset to the selected clip in one undoable edit",
           { expect(applies_a_preset_in_one_edit()).toBeTruthy(); });
    });
}
