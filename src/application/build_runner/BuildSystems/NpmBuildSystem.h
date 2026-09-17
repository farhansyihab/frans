// ~/AgenAI/frans/src/application/build_runner/BuildSystems/NpmBuildSystem.h
#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

// Build system untuk project npm (package.json).
//
// Commands:
//   Build: npm run build   (hanya kalau scripts.build ada di package.json)
//   Test:  npm test        (hanya kalau scripts.test ada)
//   Run:   di-skip (user jalankan sendiri via npm start)
//
// Catatan: npm test default dari npm init adalah script yang cuma
// print "Error: no test specified" dan exit code 1. Kita deteksi
// via package.json apakah scripts.test benar-benar didefinisikan.
class NpmBuildSystem : public IBuildSystem {
public:
    std::string name() const override { return "npm"; }

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