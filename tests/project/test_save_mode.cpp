#include "oma/project/document.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>

#include "oma_test.hpp"

// Code review 2026-10-14: every save set 0644, so a project restricted to 0600 became
// readable by every local account at the next save or autosave.

namespace fs = std::filesystem;

namespace {

mode_t mode_of(const fs::path& file) {
    struct stat st{};
    return ::stat(file.c_str(), &st) == 0 ? (st.st_mode & 07777) : 0;
}

void keeps_permissions() {
    const fs::path dir =
        fs::temp_directory_path() / ("oma-save-mode-" + std::to_string(::getpid()));
    fs::create_directories(dir);
    const fs::path file = dir / "private.omamovie";
    const oma::project::Document doc;
    expect(oma::project::save(doc, file).has_value()).toBeTruthy();
    // A new file gets 0666 under the umask, like any file the user creates.
    const mode_t mask = ::umask(022); // single-threaded test: reading the mask is safe here
    ::umask(mask);
    expect(static_cast<int>(mode_of(file))).toBe(static_cast<int>(0666 & ~mask));
    ::chmod(file.c_str(), 0600);
    expect(oma::project::save(doc, file).has_value()).toBeTruthy();
    expect(static_cast<int>(mode_of(file))).toBe(0600);
    fs::remove_all(dir);
}

} // namespace

void run_save_mode_tests() {
    describe("project::save permissions", {
        it("keeps a restricted file restricted and honors the umask", { keeps_permissions(); });
    });
}
