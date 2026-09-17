#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

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