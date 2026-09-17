#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

// Fallback kalau tidak ada marker build di project_root.
//
//   Python    : Build skip, Test pytest, Run python3 <entry>
//   C++       : Build g++ -std=c++20, Test skip, Run ./a.out
//   TypeScript: Build tsc --noEmit, Test skip, Run skip
//   JavaScript: Build node --check, Test skip, Run node <entry>
class StandaloneBuildSystem : public IBuildSystem {
public:
    std::string name() const override { return "Standalone"; }

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

private:
    static std::string findPythonEntry(const std::vector<GeneratedFile>& files);
    static std::string findCppEntry   (const std::vector<GeneratedFile>& files);
    static std::string findTsEntry    (const std::vector<GeneratedFile>& files);
    static std::string findJsEntry    (const std::vector<GeneratedFile>& files);
};

} // namespace frans::application