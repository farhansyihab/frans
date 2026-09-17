// ~/AgenAI/frans/src/application/build_runner/BuildSystems/StandaloneBuildSystem.h
#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

// Fallback build system — dipakai kalau tidak ada marker build di project_root.
//
// Perilaku (per bahasa dominan):
//
//   Python:
//     Build: skip (syntax check opsional via py_compile, terlalu ribet)
//     Test:  python3 -m pytest -q  (kalau ada test_*.py)
//     Run:   python3 <entry>
//
//   C++:
//     Build: g++ -std=c++20 -o a.out <files>
//            atau g++ -std=c++20 -fsyntax-only <files>  kalau tidak ada main
//     Test:  skip
//     Run:   ./a.out  (hanya kalau build menghasilkan binary)
//
//   TypeScript:
//     Build: npx --no-install tsc --noEmit <entry>
//     Test:  skip
//     Run:   skip
//
//   JavaScript:
//     Build: node --check <entry>
//     Test:  skip
//     Run:   node <entry>
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
    // Helper: cari entry point per bahasa.
    static std::string findPythonEntry(
        const std::vector<GeneratedFile>& files);
    static std::string findCppEntry(
        const std::vector<GeneratedFile>& files);
    static std::string findTsEntry(
        const std::vector<GeneratedFile>& files);
    static std::string findJsEntry(
        const std::vector<GeneratedFile>& files);
};

} // namespace frans::application