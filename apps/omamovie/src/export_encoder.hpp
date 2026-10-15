#pragma once

#include "oma/base/bounded_queue.hpp"
#include "oma/base/error.hpp"
#include "oma/base/jobs.hpp"
#include "oma/media/video_writer.hpp"

#include <cstdint>
#include <optional>
#include <vector>

// The export's encoder as its own stage (code review part 5): the export job renders and reads
// back frame n+1 while this stage encodes frame n, so a frame costs about the slower of the two
// instead of their sum. Same-machine comparison:
// Docs/implementation-plan-cross-editor-benchmarks.md.
//
// Threading: one worker of its own pool (no ad hoc thread, §13) is the only user of the writer
// until finish() returns. Frames move in through a queue of two (backpressure); the producer
// never touches them again. An encoder error closes the queue, so the next push fails and the
// producer collects the error from finish(). Destroying the stage closes the queue and joins the
// worker: declare it after the writer so it goes first.
struct EncodedFrame {
    // RGB24 (CPU reference), RGBA8 (GPU readback for libx264) or NV12 (GPU readback for VA-API).
    enum class Pixels : std::uint8_t { Rgb, Rgba, Nv12 } kind = Pixels::Rgb;
    std::vector<std::uint8_t> pixels; // one frame, packed rows
    int stride = 0;                   // NV12 only: bytes per row of either plane
    std::vector<float> sound;         // this frame's interleaved stereo samples
};

class EncodeStage {
public:
    EncodeStage(oma::media::VideoWriter& writer, int width);
    ~EncodeStage();
    EncodeStage(const EncodeStage&) = delete;
    EncodeStage& operator=(const EncodeStage&) = delete;

    // False once the encoder failed; finish() then says why.
    [[nodiscard]] bool push(EncodedFrame frame);
    // Encodes what is queued and stops: the first encoder error, or success. Call once.
    [[nodiscard]] oma::Result<void> finish();
    // Milliseconds the worker spent writing video and sound; valid after finish().
    [[nodiscard]] double encode_ms() const noexcept;

private:
    oma::Result<void> run();

    oma::media::VideoWriter& writer_;
    int width_;
    double encode_ms_ = 0; // written by the worker, read after finish() joined it
    oma::BoundedQueue<EncodedFrame> queue_{2};
    oma::JobPool pool_{1};
    oma::JobHandle job_;
    std::optional<oma::Result<void>> result_;
};
