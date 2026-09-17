// ~/AgenAI/frans/src/application/build_runner/IBuildSystem.h
#pragma once

#include "application/agent/MultiFileParser.h"        // GeneratedFile
#include "application/build_runner/BuildReport.h"
#include "application/build_runner/ProjectInfo.h"
#include "domain/fs/IFileSystemPort.h"

#include <string>
#include <vector>

namespace frans::application {

// Satu command dalam satu phase. Label untuk UI.
struct PhaseCommand {
    std::string              label;   // "configure", "compile", "test", "run"
    std::vector<std::string> argv;    // argv[0] = executable
};

// Interface untuk build system.
//
// Kontrak:
//   - Semua method TIDAK boleh throw.
//   - Return vector kosong = skip phase.
//   - Semua command dijalankan dengan cwd = ProjectInfo::project_root.
//
// Implementasi konkret:
//   - CMakeBuildSystem
//   - MakeBuildSystem
//   - MesonBuildSystem
//   - NpmBuildSystem
//   - PoetryBuildSystem
//   - StandaloneBuildSystem (fallback)
class IBuildSystem {
public:
    virtual ~IBuildSystem() = default;

    // Nama untuk UI: "CMake", "Make", dst.
    virtual std::string name() const = 0;

    // Test runner name override (kosong = pakai ProjectInfo::test_runner).
    // Default: kosong (pakai yang sudah dideteksi ProjectDetector).
    virtual std::string testRunnerName(const ProjectInfo& info) const {
        (void)info;
        return {};
    }

    // Command per phase. Vector kosong = skip phase.
    virtual std::vector<PhaseCommand> buildCommands(
        const ProjectInfo& info,
        const std::vector<GeneratedFile>& files,
        frans::domain::IFileSystemPort& fs) const = 0;

    virtual std::vector<PhaseCommand> testCommands(
        const ProjectInfo& info,
        frans::domain::IFileSystemPort& fs) const = 0;

    // runCommands dipanggil HANYA kalau build sukses (dan test sukses kalau
    // ada test runner). Return vector kosong = skip phase Run.
    virtual std::vector<PhaseCommand> runCommands(
        const ProjectInfo& info,
        const std::vector<GeneratedFile>& files) const = 0;

    // Parse stderr dari semua command dalam phase, isi error_message +
    // related_files. Dipanggil oleh BuildRunner setelah phase selesai.
    virtual void parseOutput(
        PhaseResult& result,
        const ProjectInfo& info) const = 0;

protected:
    // Helper: strip prefix project_root dari path absolut.
    static std::string stripRoot(const std::string& raw,
                                  const std::string& root)
    {
        if (root.empty() || raw.empty()) return raw;
        std::string prefix = root;
        if (prefix.back() != '/') prefix += '/';
        if (raw.size() > prefix.size() && raw.rfind(prefix, 0) == 0) {
            return raw.substr(prefix.size());
        }
        return raw;
    }

    // Helper: ambil baris non-empty terakhir (untuk pesan error default).
    static std::string lastNonEmptyLine(const std::string& s) {
        std::string line, last;
        for (std::size_t p = 0; p < s.size(); ) {
            const std::size_t nl = s.find('\n', p);
            if (nl == std::string::npos) {
                line = s.substr(p);
                p = s.size();
            } else {
                line = s.substr(p, nl - p);
                p = nl + 1;
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) last = line;
        }
        return last;
    }

    // Helper: tail string dengan limit.
    static std::string tailString(const std::string& s, std::size_t max_chars) {
        if (s.size() <= max_chars) return s;
        return std::string("...(truncated)...\n")
             + s.substr(s.size() - max_chars);
    }
};

} // namespace frans::application