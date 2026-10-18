#include "export_encoder.hpp"

#include <chrono>

EncodeStage::EncodeStage(oma::media::VideoWriter& writer, int width)
    : writer_(writer), width_(width) {
    job_ = pool_.submit("export-encode", [this](oma::JobContext&) { return run(); });
}

EncodeStage::~EncodeStage() {
    queue_.close(); // wakes the worker; pool_ then joins it
}

bool EncodeStage::push(EncodedFrame frame) {
    return queue_.push(std::move(frame));
}

oma::Result<void> EncodeStage::finish() {
    if (!result_) {
        queue_.close(); // the worker drains what is queued, then sees the end
        result_ = job_.wait();
    }
    return *result_;
}

double EncodeStage::encode_ms() const noexcept {
    return encode_ms_;
}

oma::Result<void> EncodeStage::run() {
    using Clock = std::chrono::steady_clock;
    while (auto frame = queue_.pop()) {
        const auto start = Clock::now();
        using P = EncodedFrame::Pixels;
        auto video = frame->kind == P::Nv12   ? writer_.write_nv12(frame->pixels, frame->stride)
                     : frame->kind == P::Rgba ? writer_.write_rgba(frame->pixels, width_ * 4)
                                              : writer_.write(frame->pixels, width_ * 3);
        auto sound = video ? writer_.write_audio(frame->sound) : video;
        encode_ms_ += std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        if (!sound) {
            queue_.close(); // the producer's next push fails and it asks finish() why
            return sound;
        }
    }
    return {};
}
