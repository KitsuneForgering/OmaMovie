#include "oma/base/jobs.hpp"

#include "oma/base/log.hpp"

#include <algorithm>
#include <exception>

namespace oma {

namespace detail {

struct JobShared {
    JobShared(std::string job_name, JobFn job_fn)
        : name(std::move(job_name)), fn(std::move(job_fn)) {}

    const std::string name;
    JobFn fn; // touched only by the worker that runs the job
    CancellationSource cancel;
    std::atomic<double> progress{0.0};

    mutable std::mutex mutex;
    mutable std::condition_variable done_cv;
    JobState state = JobState::Pending; // guarded by mutex
    std::optional<Error> error;         // guarded by mutex

    void set_running() {
        const std::scoped_lock lock(mutex);
        state = JobState::Running;
    }

    void finish(JobState final_state, std::optional<Error> final_error) {
        {
            const std::scoped_lock lock(mutex);
            state = final_state;
            error = std::move(final_error);
        }
        done_cv.notify_all();
    }

    void finish_cancelled() {
        finish(JobState::Cancelled, Error(ErrorCode::Cancelled, Category::Base,
                                          "job cancelled before it started", name));
    }
};

} // namespace detail

namespace {
bool is_terminal(JobState s) {
    return s == JobState::Succeeded || s == JobState::Failed || s == JobState::Cancelled;
}
} // namespace

// ---------------------------------------------------------------- JobContext

void JobContext::report_progress(double fraction) noexcept {
    progress_.store(std::clamp(fraction, 0.0, 1.0), std::memory_order_relaxed);
}

// ---------------------------------------------------------------- JobHandle

const std::string& JobHandle::name() const noexcept {
    static const std::string empty;
    return shared_ ? shared_->name : empty;
}

JobState JobHandle::state() const {
    if (!shared_) {
        return JobState::Cancelled;
    }
    const std::scoped_lock lock(shared_->mutex);
    return shared_->state;
}

double JobHandle::progress() const noexcept {
    return shared_ ? shared_->progress.load(std::memory_order_relaxed) : 0.0;
}

void JobHandle::cancel() noexcept {
    if (shared_) {
        shared_->cancel.cancel();
    }
}

Result<void> JobHandle::wait() const {
    if (!shared_) {
        return make_error(ErrorCode::InvalidArgument, Category::Base,
                          "waiting on an empty job handle");
    }
    std::unique_lock lock(shared_->mutex);
    shared_->done_cv.wait(lock, [this] { return is_terminal(shared_->state); });
    if (shared_->error) {
        return std::unexpected(*shared_->error);
    }
    return {};
}

// ---------------------------------------------------------------- JobPool

JobPool::JobPool(std::size_t worker_count) : worker_count_(std::max<std::size_t>(1, worker_count)) {
    workers_.reserve(worker_count_);
    for (std::size_t i = 0; i < worker_count_; ++i) {
        workers_.emplace_back([this](const std::stop_token& stop) { worker_loop(stop); });
    }
}

JobPool::~JobPool() {
    shutdown();
}

JobHandle JobPool::submit(std::string name, JobFn fn) {
    auto job = std::make_shared<detail::JobShared>(std::move(name), std::move(fn));
    {
        const std::scoped_lock lock(mutex_);
        if (accepting_) {
            queue_.push_back(job);
            cv_.notify_one();
            return JobHandle(std::move(job));
        }
    }
    job->cancel.cancel();
    job->finish_cancelled();
    return JobHandle(std::move(job));
}

void JobPool::shutdown() noexcept {
    {
        const std::scoped_lock lock(mutex_);
        accepting_ = false;
        for (auto& job : queue_) {
            job->cancel.cancel();
            job->finish_cancelled();
        }
        queue_.clear();
        for (auto& job : running_) {
            job->cancel.cancel();
        }
    }
    for (auto& worker : workers_) {
        worker.request_stop();
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void JobPool::worker_loop(const std::stop_token& stop) {
    for (;;) {
        std::shared_ptr<detail::JobShared> job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, stop, [this] { return !queue_.empty(); });
            if (queue_.empty()) {
                return; // stop requested and nothing left
            }
            job = std::move(queue_.front());
            queue_.pop_front();
            if (job->cancel.is_cancelled()) {
                job->finish_cancelled();
                continue;
            }
            running_.push_back(job);
        }

        job->set_running();
        JobContext ctx(job->cancel.token(), job->progress);
        Result<void> result;
        try {
            result = job->fn(ctx);
        } catch (const std::exception& e) {
            result = make_error(ErrorCode::Internal, Category::Base,
                                std::string("job threw an exception: ") + e.what(), job->name);
        } catch (...) {
            result = make_error(ErrorCode::Internal, Category::Base,
                                "job threw a non-standard exception", job->name);
        }

        {
            const std::scoped_lock lock(mutex_);
            std::erase(running_, job);
        }

        if (result) {
            job->progress.store(1.0, std::memory_order_relaxed);
            job->finish(JobState::Succeeded, std::nullopt);
        } else if (result.error().code() == ErrorCode::Cancelled) {
            job->finish(JobState::Cancelled, std::move(result.error()));
        } else {
            log_warn(Category::Base, "job '{}' failed: {}", job->name, result.error().summary());
            job->finish(JobState::Failed, std::move(result.error()));
        }
    }
}

} // namespace oma
