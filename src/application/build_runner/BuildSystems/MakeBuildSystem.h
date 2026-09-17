// ~/AgenAI/frans/src/application/build_runner/BuildSystems/MakeBuildSystem.h
#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

// Build system untuk Makefile tradisional.
//
// Commands:
//   Build: make -jN
//   Test:  make test  (hanya kalau target test ada)
//   Run:   di-skip
class MakeBuildSystem : public IBuildSystem {
public:
    std::string name() const override { return "Make"; }

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