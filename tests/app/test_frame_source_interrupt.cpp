#include "frame_source.hpp"

#include <QFileInfo>

#include "oma_test.hpp"

namespace {
// A software FrameSource asked for a frame deep into a GOP while the interrupt fires: Cancelled,
// and the next request (no interrupt) gets the exact frame, so the half-sought stream was dropped.
bool recovers_after_an_interrupted_seek() {
    FrameSource frames(nullptr);
    const std::string path =
        QFileInfo("tests/fixtures/generated/h264_29.97fps.mp4").absoluteFilePath().toStdString();
    // 20 frames at 30000/1001: 20020/30000 s.
    const auto t = oma::RationalTime::make(20020, oma::Rational::literal(1, 30000)).value();
    bool fire = true;
    frames.set_interrupt([&fire] { return fire; });
    const auto stopped = frames.picture_at(path, t);
    if (stopped || stopped.error().code() != oma::ErrorCode::Cancelled)
        return false;
    fire = false;
    frames.set_interrupt({});
    const auto shown = frames.picture_at(path, t);
    return shown && shown->frame->pts() && *shown->frame->pts() == t;
}
} // namespace

void run_frame_source_interrupt_tests() {
    describe("Interrupted viewer seeks", {
        it("drops a stream stopped mid-seek and finds the exact frame next time",
           { expect(recovers_after_an_interrupted_seek()).toBeTruthy(); });
    });
}
