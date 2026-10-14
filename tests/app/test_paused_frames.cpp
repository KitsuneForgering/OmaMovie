// Paused viewer requests (async interactive work, ADR-0020): decoded on the job worker, in
// hardware where the device allows it; the latest request wins and nothing stale is shown.
#include "preview_item.hpp"
#include "session.hpp"
#include "wait_for.hpp"

#include "oma/gpu/device.hpp"

#include <QElapsedTimer>
#include <QFileInfo>

#include <algorithm>
#include <memory>
#include <vector>

#include "oma_test.hpp"

namespace {
// A viewer on `device` (hardware forced on when there is one) and a session showing a 10 s GOP.
struct Bench {
    explicit Bench(const oma::gpu::Device* device) {
        qputenv("OMA_PREVIEW_HARDWARE", device != nullptr ? "1" : "0");
        item.setDevice(device);
        session = std::make_unique<Session>();
        session->setSilent(true);
        session->setPreview(&item);
        session->open(QFileInfo("tests/fixtures/generated/h264_long_gop.mp4").absoluteFilePath());
        ready = wait_for([&] { return session->clips().size() == 1 && !session->importsPending(); });
    }
    ~Bench() {
        session.reset(); // before the item it points to
        qunsetenv("OMA_PREVIEW_HARDWARE");
    }
    Bench(const Bench&) = delete;
    Bench& operator=(const Bench&) = delete;
    Bench(Bench&&) = delete;
    Bench& operator=(Bench&&) = delete;
    PreviewItem item;
    std::unique_ptr<Session> session;
    bool ready = false;
};

std::unique_ptr<oma::gpu::Device> gpu() {
    auto created = oma::gpu::Device::create();
    return created ? std::move(*created) : nullptr;
}

// Scrubbing back and forth: the frames the viewer is given follow the order of the requests
// (never an older one after a newer one) and the last request is what stays on screen.
bool latest_request_wins(const oma::gpu::Device* device) {
    Bench b(device);
    if (!b.ready) return false;
    std::vector<std::int64_t> asked;
    std::ptrdiff_t newest_seen = -1;
    bool ordered = true;
    for (const double s : {9.6, 0.7, 8.3, 2.0, 6.6, 3.3, 5.0}) {
        b.session->seek(s);
        asked.push_back(b.session->frame());
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        const auto at = std::ranges::find(asked, b.item.pendingFrame());
        if (at == asked.end()) continue; // nothing shown yet
        ordered = ordered && at - asked.begin() >= newest_seen;
        newest_seen = std::max(newest_seen, at - asked.begin());
    }
    const bool settled = wait_for([&] { return b.item.pendingFrame() == asked.back(); }, 5000);
    return ordered && settled && !b.session->failed();
}

// Leaving the project, or the session, while a deep seek decodes: no frame of the old project
// appears, nothing fails and destruction does not wait for a GOP.
bool abandons_a_seek_in_flight(const oma::gpu::Device* device) {
    Bench b(device);
    if (!b.ready) return false;
    b.session->seek(9.6);
    b.session->newProject();
    wait_for([] { return false; }, 1000);
    const bool switched = b.item.pendingFrame() == -1 && !b.session->failed();
    b.session->open(QFileInfo("tests/fixtures/generated/h264_long_gop.mp4").absoluteFilePath());
    wait_for([&] { return b.session->clips().size() == 1 && !b.session->importsPending(); });
    b.session->seek(9.6);
    QElapsedTimer t;
    t.start();
    b.session.reset();
    return switched && t.elapsed() < 2000;
}

// A lost device: requests stop without touching it, the session does not fail and the user is
// told the viewer stopped.
bool survives_device_loss(const oma::gpu::Device& device) {
    Bench b(&device);
    if (!b.ready) return false;
    device.mark_lost();
    b.session->seek(5.0);
    wait_for([&] { return !b.session->notice().isEmpty(); }, 3000);
    return !b.session->failed() && b.session->notice().contains("graphics device");
}
} // namespace

void run_paused_frames_tests() {
    describe("Paused viewer frames", {
        it("publish only the latest request (software)", { expect(latest_request_wins(nullptr)).toBeTruthy(); });
        it("abandon a seek on project switch and destruction (software)",
           { expect(abandons_a_seek_in_flight(nullptr)).toBeTruthy(); });
        auto device = gpu();
        if (!device) return; // no Vulkan here: the hardware cases need a device
        it("publish only the latest request (device)", { expect(latest_request_wins(device.get())).toBeTruthy(); });
        it("abandon a seek on project switch and destruction (device)",
           { expect(abandons_a_seek_in_flight(device.get())).toBeTruthy(); });
        it("survive a lost device", { expect(survives_device_loss(*device)).toBeTruthy(); });
    });
}
