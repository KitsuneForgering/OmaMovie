// Frame lifetime and shutdown order under a concurrent producer (CLAUDE.md §8.1, §13): a job
// decodes into a bounded queue while this thread composites, then playback is cancelled
// mid-stream. Queued frames outlive their decoder and are still composited; destruction follows
// consumers -> queue -> job pool -> device. Run under ASan and TSan to make it meaningful.
#include "oma/base/bounded_queue.hpp"
#include "oma/base/jobs.hpp"
#include "oma/compositor/compositor.hpp"
#include "oma/gpu/device.hpp"
#include "oma/media/video_decoder.hpp"

#include "compositor_test.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <memory>
#include <optional>
#include <thread>

#include "oma_test.hpp"

namespace {

using oma::BoundedQueue;
using oma::JobContext;
using oma::JobPool;
using oma::Result;
using oma::compositor::LayerInput;
using oma::compositor::RenderGraph;
using oma::compositor::VulkanCompositor;
using oma::media::DecodePath;
using oma::media::VideoDecoder;
using oma::media::VideoDecoderOptions;
using oma::media::VideoFrame;

struct Outcome {
    bool ran = false;
    int rendered = 0;
    int drained = 0;
    bool render_failed = false;
    bool producer_stopped = false;
    bool any_on_gpu = false;
};

Outcome produce_consume_cancel(bool hardware) {
    Outcome out;
    // Its own device, so the test controls the last step of the shutdown order.
    auto device = oma::gpu::Device::create();
    if (!device) {
        return out;
    }
    VideoDecoderOptions options;
    options.device = device->get();
    if (!hardware) {
        options.paths = {DecodePath::Software};
    }
    auto probe = VideoDecoder::open(fixture("h264_30fps_aac.mp4"), options);
    if (!probe || (hardware && (*probe)->path() == DecodePath::Software)) {
        return out; // no hardware path here
    }
    const oma::media::ColorInfo color = (*probe)->stream().video->color;
    probe->reset();
    out.ran = true;

    auto compositor = VulkanCompositor::create(**device);
    auto queue = std::make_unique<BoundedQueue<VideoFrame>>(4);
    auto pool = std::make_unique<JobPool>(1);
    std::atomic<bool> producer_done = false;

    // The decoder lives and dies on the worker; its frames cross threads by move.
    auto job = pool->submit("decode", [&](JobContext& ctx) -> Result<void> {
        auto decoder = VideoDecoder::open(fixture("h264_30fps_aac.mp4"), options);
        if (!decoder) {
            producer_done = true;
            return std::unexpected(decoder.error());
        }
        while (!ctx.is_cancelled()) {
            auto f = (*decoder)->next();
            if (!f) {
                producer_done = true;
                return std::unexpected(f.error());
            }
            if (!*f) { // loop the file so cancellation always lands mid-stream
                if (auto r = (*decoder)->seek({}); !r) {
                    producer_done = true;
                    return r;
                }
                continue;
            }
            if (!queue->push(std::move(**f))) {
                break; // closed: shutting down
            }
        }
        producer_done = true;
        return {};
    });

    RenderGraph graph;
    graph.width = 320;
    graph.height = 180;
    graph.layers.resize(1);
    const auto render = [&](VideoFrame& frame) {
        out.any_on_gpu = out.any_on_gpu || frame.on_gpu();
        const std::array<LayerInput, 1> inputs{LayerInput{.frame = &frame, .color = color}};
        if (!compositor || !(*compositor)->render(graph, inputs)) {
            out.render_failed = true;
        }
    };
    while (out.rendered < 40 && !producer_done) {
        if (auto frame = queue->try_pop()) {
            render(*frame);
            ++out.rendered;
        } else {
            std::this_thread::yield();
        }
    }

    // Let the queue fill so frames are left to outlive the decoder, whichever side is faster.
    while (queue->size() < queue->capacity() && !producer_done) {
        std::this_thread::yield();
    }
    // Cancel mid-stream: request cancellation, release a producer blocked on a full queue, join.
    job.cancel();
    queue->close();
    static_cast<void>(job.wait());
    out.producer_stopped = producer_done;
    pool->shutdown();

    // The decoder is gone; its queued frames must still be valid to composite.
    while (auto frame = queue->try_pop()) {
        render(*frame);
        ++out.drained;
    }
    if (compositor) {
        compositor->reset(); // consumers
    }
    queue.reset();   // frames (none left)
    pool.reset();    // workers
    device->reset(); // device last
    return out;
}

void check(bool hardware) {
    const Outcome o = produce_consume_cancel(hardware);
    if (!o.ran) {
        std::printf("    (skipped: no %s decode path)\n", hardware ? "hardware" : "Vulkan");
        return;
    }
    std::printf("    %s: %d rendered while decoding, %d drained after cancellation\n",
                hardware ? "hardware" : "software", o.rendered, o.drained);
    expect(o.rendered).toEqual(40);
    expect(o.drained).toEqual(4); // the full queue, composited after the decoder was destroyed
    expect(o.producer_stopped).toBeTruthy();
    expect(o.render_failed).toBeFalsy();
    expect(o.any_on_gpu == hardware).toBeTruthy();
}

} // namespace

void run_lifetime_tests() {
    describe("frame lifetime under a concurrent producer", {
        it("cancels software decoding mid-stream and shuts down in order", { check(false); });
        it("cancels hardware decoding mid-stream and shuts down in order", { check(true); });
    });
}
