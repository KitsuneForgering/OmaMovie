#include "oma/audio/output.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "oma_test.hpp"

using oma::audio::AudioOutput;
using oma::audio::OutputFormat;

namespace {

using std::chrono::milliseconds;

OutputFormat stereo48k() {
    return OutputFormat{};
}

std::unique_ptr<AudioOutput> null_output() {
    return oma::audio::make_null_output(stereo48k(), milliseconds(2)).value();
}

void write_silence(AudioOutput& out, std::size_t frames) {
    const std::vector<float> silence(frames * static_cast<std::size_t>(out.format().channels),
                                     0.0F);
    out.ring().write(silence);
}

// Polls until `done` holds or the deadline passes (sanitizer builds run slowly).
template <typename Pred>
bool wait_for(Pred done, milliseconds deadline = milliseconds(5000)) {
    const auto until = std::chrono::steady_clock::now() + deadline;
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        std::this_thread::sleep_for(milliseconds(1));
    }
    return true;
}

bool null_consumes_everything_written() {
    auto out = null_output();
    write_silence(*out, 2400); // 50 ms
    if (!out->start()) {
        return false;
    }
    const bool drained = wait_for([&] { return out->frames_consumed() == 2400; });
    out->stop();
    return drained && out->ring().readable() == 0;
}

bool null_counts_underruns_without_advancing() {
    auto out = null_output();
    if (!out->start()) {
        return false;
    }
    const bool underran = wait_for([&] { return out->underruns() > 0; });
    out->stop();
    return underran && out->frames_consumed() == 0;
}

// The flush is asynchronous: the consumer drops what was queued at its next pull and keeps
// what is written after the call (a restart's new audio).
bool null_flush_drops_queued_frames() {
    auto out = null_output();
    write_silence(*out, 48000); // one second, far more than the test runs
    if (!out->start()) {
        return false;
    }
    out->flush();
    const std::int64_t at_flush = out->frames_consumed();
    write_silence(*out, 4800); // 100 ms written after the flush
    std::this_thread::sleep_for(milliseconds(300));
    const std::int64_t played = out->frames_consumed() - at_flush;
    const bool drained = out->ring().readable() == 0;
    out->stop();
    // The new 100 ms play; of the old second at most what one pull took before the discard.
    return drained && played >= 4800 && played < 4800 + 4800;
}

bool null_consumes_at_the_device_rate() {
    auto out = null_output();
    write_silence(*out, 48000);
    const auto start = std::chrono::steady_clock::now();
    if (!out->start()) {
        return false;
    }
    std::this_thread::sleep_for(milliseconds(100));
    const std::int64_t consumed = out->frames_consumed();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    out->stop();
    const auto elapsed_frames =
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() * 48 / 1000;
    // Never ahead of real time; at least half of it even on a loaded sanitizer run.
    return consumed <= elapsed_frames && consumed >= 4800 / 2;
}

} // namespace

void run_output_tests() {
    describe("NullOutput", {
        it("rejects an invalid format", {
            OutputFormat bad;
            bad.channels = 0;
            expect(oma::audio::make_null_output(bad).has_value()).toBeFalsy();
        });

        it("consumes everything written",
           { expect(null_consumes_everything_written()).toBeTruthy(); });

        it("counts underruns and stalls the clock without data",
           { expect(null_counts_underruns_without_advancing()).toBeTruthy(); });

        it("drops queued frames on flush",
           { expect(null_flush_drops_queued_frames()).toBeTruthy(); });

        it("consumes frames no faster than real time",
           { expect(null_consumes_at_the_device_rate()).toBeTruthy(); });

        it("flushes and stops safely when never started", {
            auto out = null_output();
            write_silence(*out, 100);
            out->flush();
            out->stop();
            expect(out->ring().readable()).toBe(0U);
        });
    });

    describe("PipeWireOutput", {
        it("plays silence through the daemon when one answers", {
            auto out = oma::audio::make_pipewire_output(stereo48k(), "OmaMovie tests");
            if (!out) {
                std::printf("  (skipping PipeWire test: %s)\n", out.error().summary().c_str());
                return;
            }
            write_silence(**out, 4800);
            expect((*out)->start().has_value()).toBeTruthy();
            // A suspended or busy graph may not pull at once; report instead of failing.
            const bool advanced =
                wait_for([&] { return (*out)->frames_consumed() > 0; }, milliseconds(2000));
            std::printf("    consumed %lld frames, latency %lld, underruns %lld\n",
                        static_cast<long long>((*out)->frames_consumed()),
                        static_cast<long long>((*out)->latency_frames()),
                        static_cast<long long>((*out)->underruns()));
            if (!advanced) {
                std::printf("    (the graph did not pull: no sink or suspended)\n");
            }
            expect((*out)->healthy()).toBeTruthy();
            (*out)->set_muted(true); // the test plays silence anyway; this exercises the control
            (*out)->flush();
            (*out)->stop();
            // Silence padded for underruns is never counted as consumed.
            expect((*out)->frames_consumed() <= 4800).toBeTruthy();
        });
    });
}
