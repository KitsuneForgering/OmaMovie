#pragma once

#include "oma/base/error.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

// Background job system (CLAUDE.md §13, ADR-0003). Every asynchronous task in the
// project goes through a JobPool; components never create threads ad hoc.

namespace oma {

// Read side of a cancellation flag. A default-constructed token is never cancelled.
class CancellationToken {
public:
    CancellationToken() = default;
    [[nodiscard]] bool is_cancelled() const noexcept {
        return flag_ && flag_->load(std::memory_order_acquire);
    }

private:
    friend class CancellationSource;
    explicit CancellationToken(std::shared_ptr<const std::atomic<bool>> flag)
        : flag_(std::move(flag)) {}

    // Shared because the owner and the running job have independent lifetimes.
    std::shared_ptr<const std::atomic<bool>> flag_;
};

class CancellationSource {
public:
    CancellationSource() : flag_(std::make_shared<std::atomic<bool>>(false)) {}

    [[nodiscard]] CancellationToken token() const { return CancellationToken(flag_); }
    void cancel() noexcept { flag_->store(true, std::memory_order_release); }
    [[nodiscard]] bool is_cancelled() const noexcept {
        return flag_->load(std::memory_order_acquire);
    }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

enum class JobState : std::uint8_t {
    Pending,
    Running,
    Succeeded,
    Failed,
    Cancelled,
};

// What a running job sees: its cancellation token and a way to report progress.
class JobContext {
public:
    JobContext(CancellationToken token, std::atomic<double>& progress) noexcept
        : token_(std::move(token)), progress_(progress) {}

    [[nodiscard]] const CancellationToken& token() const noexcept { return token_; }
    [[nodiscard]] bool is_cancelled() const noexcept { return token_.is_cancelled(); }

    // Fraction in [0, 1]; values outside are clamped.
    void report_progress(double fraction) noexcept;

private:
    CancellationToken token_;
    std::atomic<double>& progress_;
};

// A job checks is_cancelled() at safe points and returns ErrorCode::Cancelled to stop.
// Jobs must not throw; an escaping exception is converted into ErrorCode::Internal.
using JobFn = std::move_only_function<Result<void>(JobContext&)>;

namespace detail {
struct JobShared;
} // namespace detail

class JobHandle {
public:
    JobHandle() = default;

    [[nodiscard]] bool valid() const noexcept { return shared_ != nullptr; }
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] JobState state() const;
    [[nodiscard]] double progress() const noexcept;

    // Requests cooperative cancellation. A pending job never starts.
    void cancel() noexcept;

    // Blocks until the job finishes. Returns the job's error, ErrorCode::Cancelled
    // for cancelled jobs, or success.
    Result<void> wait() const;

private:
    friend class JobPool;
    explicit JobHandle(std::shared_ptr<detail::JobShared> shared) : shared_(std::move(shared)) {}

    std::shared_ptr<detail::JobShared> shared_;
};

class JobPool {
public:
    explicit JobPool(std::size_t worker_count);
    ~JobPool();

    JobPool(const JobPool&) = delete;
    JobPool& operator=(const JobPool&) = delete;
    JobPool(JobPool&&) = delete;
    JobPool& operator=(JobPool&&) = delete;

    // After shutdown() the returned handle is already Cancelled.
    [[nodiscard]] JobHandle submit(std::string name, JobFn fn);

    // Idempotent. Stops accepting jobs, cancels pending jobs without running them,
    // requests cancellation of running jobs and joins every worker.
    // Must not be called from inside a job.
    void shutdown() noexcept;

    [[nodiscard]] std::size_t worker_count() const noexcept { return worker_count_; }

private:
    void worker_loop(const std::stop_token& stop);

    std::size_t worker_count_;
    std::mutex mutex_;
    std::condition_variable_any cv_;
    std::deque<std::shared_ptr<detail::JobShared>> queue_;
    std::vector<std::shared_ptr<detail::JobShared>> running_;
    bool accepting_ = true;
    std::vector<std::jthread> workers_;
};

} // namespace oma
