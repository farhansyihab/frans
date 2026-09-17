#pragma once

#include <string>
#include <vector>

namespace frans::application {

enum class BuildSystemKind {
    CMake,
    Make,
    Meson,
    Npm,
    Poetry,
    Standalone,
};

const char* buildSystemKindName(BuildSystemKind k);

struct ProjectInfo {
    std::string               sandbox_root;
    std::string               project_root;
    BuildSystemKind           kind           = BuildSystemKind::Standalone;
    std::string               test_runner;
    std::string               primary_language;
    std::vector<std::string>  markers_found;
    std::vector<std::string>  imported_absolute_paths;
    std::vector<std::string>  imported_relative_paths;
};

} // namespace frans::application