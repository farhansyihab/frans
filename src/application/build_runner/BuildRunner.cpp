// Ganti seluruh isi BuildRunner.cpp dengan ini. Perubahan ada di bagian
// "Inisialisasi label phase" — komentar jelas menandai.

#include "application/build_runner/BuildRunner.h"

#include "application/build_runner/BuildSystems/CMakeBuildSystem.h"
#include "application/build_runner/BuildSystems/MakeBuildSystem.h"
#include "application/build_runner/BuildSystems/MesonBuildSystem.h"
#include "application/build_runner/BuildSystems/NpmBuildSystem.h"
#include "application/build_runner/BuildSystems/PoetryBuildSystem.h"
#include "application/build_runner/BuildSystems/StandaloneBuildSystem.h"
#include "application/build_runner/ProjectDetector.h"
#include "application/safety/DangerCheckHandler.h"
#include "infrastructure/system/ProcessExecutor.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace frans::application {

namespace fs = std::filesystem;

namespace {
constexpr const char* kLogPrefix = "[frans][build_runner]";
} // namespace

const char* buildPhaseName(BuildPhase p) {
    switch (p) {
        case BuildPhase::Build: return "BUILD";
        case BuildPhase::Test:  return "TEST";
        case BuildPhase::Run:   return "RUN";
    }
    return "?";
}

BuildSystemKind BuildRunner::detectBuildSystemKind(const std::string& root) {
    std::error_code ec;
    if (fs::exists(fs::path(root) / "CMakeLists.txt", ec) && !ec)
        return BuildSystemKind::CMake;
    if (fs::exists(fs::path(root) / "meson.build", ec) && !ec)
        return BuildSystemKind::Meson;
    if ((fs::exists(fs::path(root) / "Makefile", ec) && !ec)
        || (fs::exists(fs::path(root) / "GNUmakefile", ec) && !ec))
        return BuildSystemKind::Make;
    if (fs::exists(fs::path(root) / "package.json", ec) && !ec)
        return BuildSystemKind::Npm;
    if (fs::exists(fs::path(root) / "pyproject.toml", ec) && !ec)
        return BuildSystemKind::Poetry;
    return BuildSystemKind::Standalone;
}

std::unique_ptr<IBuildSystem> BuildRunner::makeBuildSystem(BuildSystemKind kind) {
    switch (kind) {
        case BuildSystemKind::CMake:      return std::make_unique<CMakeBuildSystem>();
        case BuildSystemKind::Make:       return std::make_unique<MakeBuildSystem>();
        case BuildSystemKind::Meson:      return std::make_unique<MesonBuildSystem>();
        case BuildSystemKind::Npm:        return std::make_unique<NpmBuildSystem>();
        case BuildSystemKind::Poetry:     return std::make_unique<PoetryBuildSystem>();
        case BuildSystemKind::Standalone: return std::make_unique<StandaloneBuildSystem>();
    }
    return nullptr;
}

std::string BuildRunner::formatArgv(const std::vector<std::string>& argv) {
    std::string s;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) s += ' ';
        s += argv[i];
    }
    return s;
}

PhaseResult BuildRunner::runPhase(
    BuildPhase phase,
    const std::vector<PhaseCommand>& commands,
    const std::string& cwd,
    int timeout_sec,
    frans::domain::IFileSystemPort& /*fs*/,
    const ProjectInfo& info,
    ConfirmCallback& confirm_cb)
{
    PhaseResult result;
    result.phase = phase;

    if (commands.empty()) {
        result.skipped = true;
        result.success = true;
        return result;
    }

    result.skipped = false;
    result.success = true;

    const auto t0 = std::chrono::steady_clock::now();

    for (const auto& pc : commands) {
        CommandResult cr;
        cr.label   = pc.label;
        cr.command = formatArgv(pc.argv);

        std::fprintf(stderr, "%s [%s] %s (cwd=%s)\n",
                     kLogPrefix, buildPhaseName(phase),
                     cr.command.c_str(), cwd.c_str());
        std::fflush(stderr);

        DangerCheckHandler danger;
        const DangerCheckResult dr = danger.checkCommand(pc.argv);

        if (dr.is_blocked()) {
            cr.success     = false;
            cr.exit_code   = -1;
            cr.stderr_tail = "BLOCKED by safety layer: " + dr.explanation;
            result.commands.push_back(std::move(cr));
            result.success = false;
            result.error_message = "Command diblokir safety layer: "
                                 + dr.explanation;
            break;
        }
        if (dr.needs_confirm()) {
            if (!confirm_cb) {
                cr.success     = false;
                cr.stderr_tail = "NeedsConfirm tapi confirm_cb kosong — ditolak";
                result.commands.push_back(std::move(cr));
                result.success = false;
                result.error_message = "Command butuh konfirmasi tapi tidak "
                                     "ada callback.";
                break;
            }
            const std::string reason =
                "Command akan dijalankan:\n\n  " + cr.command
                + "\n\nPattern berisiko: " + dr.explanation
                + "\n\nLanjutkan?";
            if (!confirm_cb(reason)) {
                cr.success     = false;
                cr.stderr_tail = "User menolak konfirmasi";
                result.commands.push_back(std::move(cr));
                result.success = false;
                result.error_message = "User menolak eksekusi.";
                break;
            }
        }

        const int eff_timeout = timeout_sec > 0 ? timeout_sec : 120;
        const auto cmd_t0 = std::chrono::steady_clock::now();

        const auto proc = frans::infrastructure::runCommand(
            pc.argv, eff_timeout, "", cwd, nullptr);

        const auto cmd_t1 = std::chrono::steady_clock::now();
        cr.duration_sec = std::chrono::duration<double>(cmd_t1 - cmd_t0).count();
        cr.exit_code    = proc.exit_code;
        cr.timed_out    = proc.timed_out;
        cr.cancelled    = proc.cancelled_by_user;
        cr.stdout_tail  = proc.stdout_text.size() > 4096
                        ? std::string("...(truncated)...\n")
                          + proc.stdout_text.substr(proc.stdout_text.size() - 4096)
                        : proc.stdout_text;
        cr.stderr_tail  = proc.stderr_text.size() > 4096
                        ? std::string("...(truncated)...\n")
                          + proc.stderr_text.substr(proc.stderr_text.size() - 4096)
                        : proc.stderr_text;

        if (proc.timed_out) {
            cr.success = false;
            result.success = false;
            result.error_message = "Timeout (" + std::to_string(eff_timeout)
                                 + "s) pada " + pc.label;
            result.commands.push_back(std::move(cr));
            break;
        }

        if (proc.exit_code != 0) {
            cr.success = false;
            result.success = false;
            result.commands.push_back(std::move(cr));
            break;
        }

        cr.success = true;
        result.commands.push_back(std::move(cr));
    }

    const auto t1 = std::chrono::steady_clock::now();
    result.duration_sec = std::chrono::duration<double>(t1 - t0).count();

    if (!result.success) {
        auto sys = makeBuildSystem(info.kind);
        if (sys) sys->parseOutput(result, info);
        if (result.error_message.empty()) {
            result.error_message = "Phase " + std::string(buildPhaseName(phase))
                                 + " gagal (exit code != 0).";
        }
    }

    return result;
}

BuildReport BuildRunner::runBuild(
    const std::vector<GeneratedFile>& files_written,
    const std::string& sandbox_root,
    frans::domain::IFileSystemPort& fs,
    int build_timeout_sec,
    int test_timeout_sec,
    int run_timeout_sec,
    ConfirmCallback confirm_cb)
{
    BuildReport report;
    report.sandbox_root = sandbox_root;

    // ---- Inisialisasi label phase ----
    // PENTING: jangan dihapus. Default PhaseResult::phase = BuildPhase::Build,
    // jadi kalau kita return lebih awal sebelum set, UI akan menampilkan
    // semua phase sebagai [BUILD].
    report.build.phase = BuildPhase::Build;
    report.test.phase  = BuildPhase::Test;
    report.run.phase   = BuildPhase::Run;

    if (files_written.empty()) {
        report.overall_success = false;
        report.failed_phase    = BuildPhase::Build;
        report.build.skipped   = false;
        report.build.success   = false;
        report.build.error_message = "Tidak ada file untuk di-build.";
        return report;
    }

    const ProjectInfo info =
        ProjectDetector::detect(files_written, sandbox_root);

    report.project_root     = info.project_root;
    report.build_system     = buildSystemKindName(info.kind);
    report.test_runner      = info.test_runner;
    report.primary_language = info.primary_language;

    auto system = makeBuildSystem(info.kind);
    if (!system) {
        report.overall_success = false;
        report.failed_phase    = BuildPhase::Build;
        report.build.skipped   = false;
        report.build.success   = false;
        report.build.error_message = "Internal: build system tidak tersedia.";
        return report;
    }

    {
        std::error_code ec;
        if (!fs::exists(info.project_root, ec)
            || !fs::is_directory(info.project_root, ec)) {
            report.overall_success = false;
            report.failed_phase    = BuildPhase::Build;
            report.build.skipped   = false;
            report.build.success   = false;
            report.build.error_message =
                "Project root tidak ditemukan: " + info.project_root;
            return report;
        }
    }

    const auto build_cmds = system->buildCommands(info, files_written, fs);
    report.build = runPhase(
        BuildPhase::Build, build_cmds, info.project_root,
        build_timeout_sec, fs, info, confirm_cb);

    if (!report.build.success) {
        report.overall_success = false;
        report.failed_phase    = BuildPhase::Build;
        return report;
    }

    if (!info.test_runner.empty()) {
        const auto test_cmds = system->testCommands(info, fs);
        report.test = runPhase(
            BuildPhase::Test, test_cmds, info.project_root,
            test_timeout_sec, fs, info, confirm_cb);

        if (!report.test.success && !report.test.skipped) {
            report.overall_success = false;
            report.failed_phase    = BuildPhase::Test;
            return report;
        }
    } else {
        report.test.phase   = BuildPhase::Test;
        report.test.skipped = true;
        report.test.success = true;
    }

    {
        const auto run_cmds = system->runCommands(info, files_written);
        report.run = runPhase(
            BuildPhase::Run, run_cmds, info.project_root,
            run_timeout_sec, fs, info, confirm_cb);

        if (!report.run.success && !report.run.skipped) {
            report.overall_success = false;
            report.failed_phase    = BuildPhase::Run;
            return report;
        }
    }

    report.overall_success = true;
    report.total_duration_sec =
        report.build.duration_sec + report.test.duration_sec
        + report.run.duration_sec;

    return report;
}

} // namespace frans::application
