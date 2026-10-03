#include "oma/base/jobs.hpp"
#include "oma/base/log.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include "oma_test.hpp"

using oma::ErrorCode;
using oma::JobContext;
using oma::JobHandle;
using oma::JobPool;
using oma::JobState;
using oma::Result;

namespace {

int state_of(const JobHandle& h) {
    return static_cast<int>(h.state());
}

int st(JobState s) {
    return static_cast<int>(s);
}

int wait_code(const JobHandle& h) {
    auto r = h.wait();
    return r ? -1 : static_cast<int>(r.error().code());
}

// Blocks a job until released, so tests control what is running and what is pending.
struct Gate {
    std::atomic<bool> open{false};
    std::atomic<bool> entered{false};

    void wait_inside() {
        entered.store(true);
        while (!open.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    void wait_until_entered() const {
        while (!entered.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

Result<void> spin_until_cancelled(JobContext& ctx) {
    while (!ctx.is_cancelled()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return oma::make_error(ErrorCode::Cancelled, oma::Category::Base, "stopped by token");
}

void quiet_logs() {
    oma::set_log_level(oma::Category::Base, oma::LogLevel::Off);
}

void normal_logs() {
    oma::set_log_level(oma::Category::Base, oma::LogLevel::Info);
}

} // namespace

void run_job_tests() {
    describe("JobPool", {
        beforeEach(quiet_logs);
        afterEach(normal_logs);

        it("runs a job and reports success with full progress", {
            JobPool pool(2);
            std::atomic<int> value{0};
            JobHandle h = pool.submit("compute", [&](JobContext& ctx) -> Result<void> {
                ctx.report_progress(0.5);
                value.store(42);
                return {};
            });
            expect(h.wait().has_value()).toBeTruthy();
            expect(value.load()).toEqual(42);
            expect(state_of(h)).toEqual(st(JobState::Succeeded));
            expect(h.progress()).toBeCloseTo(1.0, 0.0001);
        });

        it("keeps the error of a failed job", {
            JobPool pool(1);
            JobHandle h = pool.submit("probe", [](JobContext&) -> Result<void> {
                return oma::make_error(ErrorCode::InvalidData, oma::Category::Media, "bad header");
            });
            expect(wait_code(h)).toEqual(static_cast<int>(ErrorCode::InvalidData));
            expect(state_of(h)).toEqual(st(JobState::Failed));
        });

        it("never starts a pending job that was cancelled", {
            JobPool pool(1);
            Gate gate;
            std::atomic<bool> second_ran{false};
            JobHandle first = pool.submit("first", [&](JobContext&) -> Result<void> {
                gate.wait_inside();
                return {};
            });
            gate.wait_until_entered();
            JobHandle second = pool.submit("second", [&](JobContext&) -> Result<void> {
                second_ran.store(true);
                return {};
            });
            second.cancel();
            gate.open.store(true);
            expect(first.wait().has_value()).toBeTruthy();
            expect(wait_code(second)).toEqual(static_cast<int>(ErrorCode::Cancelled));
            expect(state_of(second)).toEqual(st(JobState::Cancelled));
            expect(second_ran.load()).toBeFalsy();
        });

        it("cancels a running job cooperatively", {
            JobPool pool(1);
            JobHandle h = pool.submit("thumbnails", spin_until_cancelled);
            while (h.state() != JobState::Running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            h.cancel();
            expect(wait_code(h)).toEqual(static_cast<int>(ErrorCode::Cancelled));
            expect(state_of(h)).toEqual(st(JobState::Cancelled));
        });

        it("shuts down with running and pending jobs", {
            std::atomic<bool> pending_ran{false};
            JobHandle running;
            JobHandle pending;
            {
                JobPool pool(1);
                running = pool.submit("waveform", spin_until_cancelled);
                while (running.state() != JobState::Running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                pending = pool.submit("proxy", [&](JobContext&) -> Result<void> {
                    pending_ran.store(true);
                    return {};
                });
                pool.shutdown();
            }
            expect(state_of(running)).toEqual(st(JobState::Cancelled));
            expect(state_of(pending)).toEqual(st(JobState::Cancelled));
            expect(pending_ran.load()).toBeFalsy();
        });

        it("refuses work after shutdown", {
            JobPool pool(1);
            pool.shutdown();
            JobHandle h = pool.submit("late", [](JobContext&) -> Result<void> { return {}; });
            expect(state_of(h)).toEqual(st(JobState::Cancelled));
        });

        it("turns an escaping exception into an internal error", {
            JobPool pool(1);
            JobHandle h = pool.submit(
                "throws", [](JobContext&) -> Result<void> { throw std::runtime_error("boom"); });
            expect(wait_code(h)).toEqual(static_cast<int>(ErrorCode::Internal));
            expect(state_of(h)).toEqual(st(JobState::Failed));
        });

        it("runs many jobs across workers", {
            JobPool pool(4);
            std::atomic<int> counter{0};
            std::vector<JobHandle> handles;
            for (int i = 0; i < 200; ++i) {
                handles.push_back(pool.submit("inc", [&](JobContext&) -> Result<void> {
                    counter.fetch_add(1);
                    return {};
                }));
            }
            for (auto& h : handles) {
                static_cast<void>(h.wait());
            }
            expect(counter.load()).toEqual(200);
        });
    });
}
