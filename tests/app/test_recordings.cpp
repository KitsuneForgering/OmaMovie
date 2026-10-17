#include "platform/omarchy/recordings.hpp"

#include <QTemporaryDir>

#include <fstream>

#include "oma_test.hpp"

namespace {
namespace fs = std::filesystem;

// Files written in shuffled age order, plus non-video files that must never be listed.
bool keeps_newest_videos_in_order() {
    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    const fs::path root = dir.path().toStdString();
    const auto base = fs::file_time_type::clock::now() - std::chrono::hours(1);
    for (const int age : {7, 2, 9, 0, 5, 1, 8, 3, 6, 4}) {
        const fs::path file = root / ("clip-" + std::to_string(age) + ".mp4");
        std::ofstream(file) << age;
        fs::last_write_time(file, base - std::chrono::minutes(age));
    }
    std::ofstream(root / "clip-0-preview.png") << 0;
    fs::create_directory(root / "folder.mkv");
    const auto top = omarchy::list_recordings(root, 4);
    if (top.size() != 4)
        return false;
    for (std::size_t i = 0; i < top.size(); ++i) {
        if (top[i].path.filename() != "clip-" + std::to_string(i) + ".mp4" || top[i].growing)
            return false;
    }
    return omarchy::list_recordings(root, 50).size() == 10 &&
           omarchy::list_recordings(root, 0).empty() &&
           omarchy::list_recordings(root / "missing", 4).empty();
}
} // namespace

void run_recordings_tests() {
    describe("Recordings", {
        it("lists the newest videos first up to the limit",
           { expect(keeps_newest_videos_in_order()).toBeTruthy(); });
    });
}
