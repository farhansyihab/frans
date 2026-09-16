// ~/AgenAI/frans/src/application/build_runner/PythonBuildStrategy.h
#pragma once

#include "application/build_runner/IBuildStrategy.h"

namespace frans::application {

// Strategy build untuk Python (Stage 11d).
//
// Entry point detection:
//   1. File .py dengan `__name__` + `__main__`.
//   2. Fallback: .py pertama (alfabetis).
//
// Build command: `python3 <entry>` (run langsung).
//
// `sandbox_root` parameter di buildCommand diabaikan — Python tidak
// butuh include path (PYTHONPATH di-supply via cwd).
class PythonBuildStrategy : public IBuildStrategy {
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