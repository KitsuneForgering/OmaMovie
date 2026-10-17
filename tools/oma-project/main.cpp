#include "commands.hpp"

#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {

int usage() {
    std::fputs("usage: oma-project inspect|validate|dump <file.omamovie>\n"
               "       oma-project import <file.otio|file.edl> [edl rate, e.g. 25 or 30000/1001]\n"
               "       oma-project convert <file.otio|file.edl> <out.omamovie> [edl rate]\n"
               "       oma-project diff <a.omamovie> <b.omamovie>\n",
               stderr);
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    const std::string_view command(argc > 1 ? argv[1] : "");
    if (argc == 4 && command == "diff")
        return run_diff(argv[2], argv[3]);
    const bool timed = (command == "import" && argc == 4) || (command == "convert" && argc == 5);
    const auto rate = timed ? rate_of(argv[argc - 1]) : oma::Rational::literal(30, 1);
    if (!rate)
        return usage();
    if (command == "convert" && (argc == 4 || argc == 5))
        return run_convert(argv[2], argv[3], *rate);
    if (command == "import" && (argc == 3 || argc == 4))
        return run_import(argv[2], *rate);
    if (argc != 3)
        return usage();
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
