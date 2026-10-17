#include "playback_clock.hpp"

#include "oma_test.hpp"

// A 30 fps sequence on a 1/48000 timebase: 1 tick per sample, 1600 per frame; 60 frames.
void run_playback_clock_tests() {
    describe("Playback clock", {
        it("follows the heard sample at 1x, one frame ahead for the render loop", {
            const auto t = tick_target(48000, 1, 1600, 0, 99.0, 30.0, 1, 59);
            expect(static_cast<int>(t.frame)).toEqual(31); // 1 s heard = frame 30, prepare 31
            expect(t.at_end).toBeFalsy();
        });
        it("follows elapsed time at other speeds, from where playback started", {
            expect(static_cast<int>(tick_target(std::nullopt, 1, 1600, 10, 0.5, 30.0, 2, 59).frame))
                .toEqual(40); // 0.5 s at 2x = 30 frames
            expect(static_cast<int>(
                       tick_target(std::nullopt, 1, 1600, 50, 1.0 / 3.0, 30.0, -1, 59).frame))
                .toEqual(40);
        });
        it("clamps to the sequence and reports the end in the playing direction", {
            const auto forward = tick_target(std::nullopt, 1, 1600, 50, 1.0, 30.0, 1, 59);
            expect(static_cast<int>(forward.frame)).toEqual(59);
            expect(forward.at_end).toBeTruthy();
            const auto backward = tick_target(std::nullopt, 1, 1600, 5, 1.0, 30.0, -4, 59);
            expect(static_cast<int>(backward.frame)).toEqual(0);
            expect(backward.at_end).toBeTruthy();
            expect(tick_target(std::nullopt, 1, 1600, 5, 0.0, 30.0, -1, 59).at_end).toBeFalsy();
        });
    });
}
