#pragma once

#include "oma/project/document.hpp"

#include <filesystem>
#include <string_view>

int run_command(std::string_view command, const oma::project::Document& doc,
                const std::filesystem::path& file);

// `import FILE.otio`: prints the import report (import_command.cpp).
int run_import(const std::filesystem::path& file);
