// ~/AgenAI/frans/src/application/build_runner/BuildSystems/PoetryBuildSystem.h
#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

// Build system untuk project Python dengan pyproject.toml.
//
// Commands:
//   Build: di-skip (Python tidak butuh build step untuk script biasa;
//          poetry build hanya untuk packaging)
//   Test:  poetry run pytest  atau  poetry run python -m pytest
//   Run:   poetry run python <entry>
class PoetryBuildSystem : public IBuildSystem {
public:
    std::string name() const override { return "Poetry"; }

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