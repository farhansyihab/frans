#pragma once

#include "application/build_runner/IBuildSystem.h"

namespace frans::application {

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

private:
    // Path `package.json` dan `node_modules/` relatif terhadap sandbox
    // root (karena fs.readFile / fs.exists bekerja relatif sandbox).
    //
    //   - project_root == sandbox_root -> ""  (root)
    //   - project_root == sandbox_root/sub -> "sub"
    //
    // Return empty string kalau project_root BUKAN subpath sandbox_root.
    static std::string relProjectToSandbox(const ProjectInfo& info);

    // Cek apakah package.json di project_root punya field "build"
    // atau "test" di scripts. Pencarian string sederhana (tidak parse
    // JSON penuh), cukup untuk deteksi.
    static bool packageJsonHasField(const std::string& json,
                                    const std::string& field);
};

} // namespace frans::application