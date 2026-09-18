#pragma once

#include "application/import/MultiFileParser.h"
#include "application/build_runner/BuildReport.h"
#include "application/build_runner/IBuildSystem.h"
#include "application/build_runner/ProjectInfo.h"
#include "domain/fs/IFileSystemPort.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace frans::application {

// Facade untuk build/test/run file yang baru diimpor.
//
// Alur:
//   1. ProjectDetector::detect() -> ProjectInfo (project_root + build system).
//   2. makeBuildSystem(info.kind) -> IBuildSystem.
//   3. Build phase -> Test phase (opsional) -> Run phase (opsional).
//
// Semua command dijalankan dengan cwd = project_root.
//
// Threading:
//   runBuild() bisa dipanggil dari worker thread. Kalau `cancel_flag`
//   non-null dan berubah jadi true, subprocess yang sedang berjalan
//   akan di-SIGKILL (POSIX) / TerminateProcess (Win32). Worst-case
//   latency: 200ms (polling interval di ProcessExecutor).
//
//   PENTING: `confirm_cb` dipanggil dari thread pemanggil runBuild().
//   Kalau dipanggil dari worker thread, JANGAN akses FLTK dari dalam
//   callback — FLTK tidak thread-safe. Untuk build system standar,
//   checkCommand() tidak pernah return NeedsConfirm, jadi confirm_cb
//   praktis tidak pernah dipanggil.
class BuildRunner {
public:
    using ConfirmCallback = std::function<bool(const std::string& reason)>;

    BuildRunner() = default;

    BuildRunner(const BuildRunner&)            = delete;
    BuildRunner& operator=(const BuildRunner&) = delete;

    BuildReport runBuild(
        const std::vector<GeneratedFile>& files_written,
        const std::string& sandbox_root,
        frans::domain::IFileSystemPort& fs,
        int build_timeout_sec = 120,
        int test_timeout_sec  = 120,
        int run_timeout_sec   = 30,
        ConfirmCallback confirm_cb = nullptr,
        std::atomic<bool>* cancel_flag = nullptr);

    static BuildSystemKind detectBuildSystemKind(const std::string& project_root);

private:
    static std::unique_ptr<IBuildSystem> makeBuildSystem(BuildSystemKind kind);

    PhaseResult runPhase(
        BuildPhase phase,
        const std::vector<PhaseCommand>& commands,
        const std::string& cwd,
        int timeout_sec,
        frans::domain::IFileSystemPort& fs,
        const ProjectInfo& info,
        ConfirmCallback& confirm_cb,
        std::atomic<bool>* cancel_flag);

    static std::string formatArgv(const std::vector<std::string>& argv);
};

} // namespace frans::application