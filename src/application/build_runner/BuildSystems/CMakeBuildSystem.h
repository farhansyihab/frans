// ~/AgenAI/frans/src/application/build_runner/BuildSystems/CMakeBuildSystem.h
#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

// Build system untuk project CMake.
//
// Commands:
//   Build: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
//          cmake --build build -jN
//   Test:  ctest --test-dir build --output-on-failure
//          (hanya kalau enable_testing/add_test ditemukan)
//   Run:   di-skip (CMake tidak punya target run standar)
//
// cwd = project_root (bukan sandbox_root, bukan folder file).
class CMakeBuildSystem : public IBuildSystem {
public:
    std::string name() const override { return "CMake"; }

    std::vector<PhaseCommand> buildCommands(
        const ProjectInfo& info,
        const std::vector<GeneratedFile>& files,
        frans::domain::IFileSystemPort& fs) const override;

    std::vector<PhaseCommand> testCommands(
        const ProjectInfo& info,
        frans::domain::IFileSystemPort& fs) const override;

    std::vector<PhaseCommand> runCommands(
        const ProjectInfo& info,
        const std::vector<GeneratedFile>& files) const override;

    void parseOutput(
        PhaseResult& result,
        const ProjectInfo& info) const override;
};

} // namespace frans::application