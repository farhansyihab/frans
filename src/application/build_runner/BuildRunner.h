// ~/AgenAI/frans/src/application/build_runner/BuildRunner.h
#pragma once

#include "application/agent/MultiFileParser.h"          // GeneratedFile
#include "application/build_runner/BuildReport.h"
#include "application/build_runner/IBuildSystem.h"
#include "application/build_runner/ProjectInfo.h"
#include "domain/fs/IFileSystemPort.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace frans::application {

class ProjectGraphCache;

// Facade untuk build/test/run file yang baru diimpor via Manual Import.
//
// Perbedaan utama dari versi lama (Stage 11d):
//   - TIDAK lagi memilih strategy berdasarkan ekstensi file.
//   - Deteksi PROJECT ROOT lewat walk-up dari direktori file.
//   - Pilih BUILD SYSTEM berdasarkan marker di project root (CMake > Meson
//     > Make > npm > Poetry > Standalone).
//   - Semua command dijalankan dengan cwd = project_root (BUKAN folder file).
//   - Multi-phase: Build → Test (opsional) → Run (opsional).
//
// Alur runBuild():
//   1. ProjectDetector::detect(files, sandbox_root) → ProjectInfo.
//   2. makeBuildSystem(info.kind) → IBuildSystem.
//   3. Build phase: system.buildCommands(info, files, fs) → jalankan
//      tiap command via ProcessExecutor (cwd = project_root).
//   4. Test phase: HANYA kalau build sukses DAN info.test_runner tidak kosong.
//   5. Run phase: HANYA kalau build sukses DAN (test sukses atau skip).
//   6. Enrich related_files via ProjectGraphCache (kalau ada error).
//   7. Return BuildReport dengan per-phase detail.
class BuildRunner {
public:
    // Callback konfirmasi user untuk command NeedsConfirm.
    // Return true = lanjut, false = tolak. nullptr + NeedsConfirm = tolak.
    using ConfirmCallback = std::function<bool(const std::string& reason)>;

    BuildRunner() = default;

    BuildRunner(const BuildRunner&)            = delete;
    BuildRunner& operator=(const BuildRunner&) = delete;

    BuildReport runBuild(
        const std::vector<GeneratedFile>& files_written,
        const std::string& sandbox_root,
        frans::domain::IFileSystemPort& fs,
        ProjectGraphCache* graph_cache,
        int build_timeout_sec = 120,
        int test_timeout_sec  = 120,
        int run_timeout_sec   = 30,
        ConfirmCallback confirm_cb = nullptr);

    // Static — deteksi build system kind dari path project root.
    // Public supaya bisa di-test tanpa instance.
    static BuildSystemKind detectBuildSystemKind(const std::string& project_root);

private:
    static std::unique_ptr<IBuildSystem> makeBuildSystem(BuildSystemKind kind);

    // Jalankan semua command dalam satu phase.
    // Return PhaseResult terisi. Stops on first failure.
    PhaseResult runPhase(
        BuildPhase phase,
        const std::vector<PhaseCommand>& commands,
        const std::string& cwd,
        int timeout_sec,
        frans::domain::IFileSystemPort& fs,
        const ProjectInfo& info,
        ConfirmCallback& confirm_cb);

    // Enrich related_files via DependencyGraph.
    void enrichRelatedFiles(
        PhaseResult& result,
        const std::string& project_root,
        const std::string& sandbox_root,
        ProjectGraphCache* graph_cache);

    static std::string formatArgv(const std::vector<std::string>& argv);
};

} // namespace frans::application