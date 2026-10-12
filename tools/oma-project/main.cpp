#include "commands.hpp"

#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {

int usage() {
    std::fputs("usage: oma-project inspect|validate|dump <file>\n", stderr);
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3)
        return usage();
    const std::string_view command(argv[1]);
    if (command != "inspect" && command != "validate" && command != "dump")
        return usage();

    const std::filesystem::path file(argv[2]);
    auto doc = oma::project::load(file);
    if (!doc) {
        std::fprintf(stderr, "%s\n", doc.error().summary().c_str());
        return 1;
    }

    return run_command(command, *doc, file);
}
