// ~/AgenAI/frans/src/application/build_runner/BuildSystems/MesonBuildSystem.h
#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

// Build system untuk Meson.
//
// Commands:
//   Build: meson setup build    (idempotent)
//          meson compile -C build
//   Test:  meson test -C build --print-errorlogs
//   Run:   di-skip
class MesonBuildSystem : public IBuildSystem {
public:
    std::string name() const override { return "Meson"; }

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