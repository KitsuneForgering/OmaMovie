#include "oma/base/log.hpp"

#include <mutex>
#include <string>
#include <vector>

#include "oma_test.hpp"

using oma::Category;
using oma::LogLevel;
using oma::LogRecord;

namespace {

struct Captured {
    LogLevel level;
    Category category;
    std::string message;
    unsigned line;
};

std::mutex g_mutex;
std::vector<Captured> g_records;

void capture(const LogRecord& r) {
    const std::scoped_lock lock(g_mutex);
    g_records.push_back(Captured{r.level, r.category, std::string(r.message), r.location.line()});
}

void install_capture() {
    {
        const std::scoped_lock lock(g_mutex);
        g_records.clear();
    }
    // Not under g_mutex: the logger calls capture() while holding its own lock.
    oma::set_log_sink(capture);
    oma::set_log_level_all(LogLevel::Info);
}

void restore_default() {
    oma::set_log_sink({});
    oma::set_log_level_all(LogLevel::Info);
}

std::size_t record_count() {
    const std::scoped_lock lock(g_mutex);
    return g_records.size();
}

Captured last_record() {
    const std::scoped_lock lock(g_mutex);
    return g_records.back();
}

} // namespace

void run_log_tests() {
    describe("Logging", {
        beforeEach(install_capture);
        afterEach(restore_default);

        it("formats messages and records category and call site", {
            oma::log_info(Category::Decode, "opened {} ({} streams)", "clip.mp4", 2);
            expect(record_count()).toEqual(1);
            const Captured c = last_record();
            expect(c.message).toEqual("opened clip.mp4 (2 streams)");
            expect(static_cast<int>(c.category)).toEqual(static_cast<int>(Category::Decode));
            expect(static_cast<int>(c.level)).toEqual(static_cast<int>(LogLevel::Info));
            expect(c.line).toBeGreaterThan(0);
        });

        it("drops records below the category level", {
            oma::log_debug(Category::Gpu, "not shown");
            expect(record_count()).toEqual(0);
            oma::set_log_level(Category::Gpu, LogLevel::Debug);
            oma::log_debug(Category::Gpu, "shown");
            oma::log_debug(Category::Audio, "still hidden");
            expect(record_count()).toEqual(1);
        });

        it("reports enabled levels without formatting", {
            oma::set_log_level(Category::Cache, LogLevel::Warn);
            expect(oma::log_enabled(Category::Cache, LogLevel::Info)).toBeFalsy();
            expect(oma::log_enabled(Category::Cache, LogLevel::Error)).toBeTruthy();
            expect(oma::log_enabled(Category::Cache, LogLevel::Off)).toBeFalsy();
        });
    });
}
