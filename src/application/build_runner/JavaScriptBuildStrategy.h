// ~/AgenAI/frans/src/application/build_runner/JavaScriptBuildStrategy.h
#pragma once

#include "application/build_runner/IBuildStrategy.h"

namespace frans::application {

// Strategy build untuk JavaScript (Stage 11d).
//
// `sandbox_root` parameter di buildCommand diabaikan — node --check
// tidak butuh include path.
class JavaScriptBuildStrategy : public IBuildStrategy {
public:
    std::string languageName() const override;

    std::string detectEntryPoint(
        const std::vector<GeneratedFile>& files,
        frans::domain::IFileSystemPort& fs,
        const std::string& sandbox_root) override;

    std::vector<std::string> buildCommand(
        const std::string& entry_point,
        const std::vector<GeneratedFile>& files,
        const std::string& sandbox_root) override;

    BuildReport parseOutput(
        int exit_code,
        bool timed_out,
        const std::string& stdout_text,
        const std::string& stderr_text,
        double duration_sec,
        const std::string& sandbox_root) override;
};

} // namespace frans::application