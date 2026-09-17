// ~/AgenAI/frans/src/application/build_runner/BuildRunner.cpp
#include "application/build_runner/BuildRunner.h"

#include "application/build_runner/BuildSystems/CMakeBuildSystem.h"
#include "application/build_runner/BuildSystems/MakeBuildSystem.h"
#include "application/build_runner/BuildSystems/MesonBuildSystem.h"
#include "application/build_runner/BuildSystems/NpmBuildSystem.h"
#include "application/build_runner/BuildSystems/PoetryBuildSystem.h"
#include "application/build_runner/BuildSystems/StandaloneBuildSystem.h"
#include "application/build_runner/ProjectDetector.h"
#include "application/project/ProjectGraphCache.h"
#include "application/safety/DangerCheckHandler.h"
#include "code_intelligence/graph/DependencyGraph.h"
#include "infrastructure/system/ProcessExecutor.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace frans::application {

namespace fs = std::filesystem;

namespace {

constexpr const char* kLogPrefix = "[frans][build_runner]";

// Helper: strip prefix `root` dari path absolut `raw`.
// Return relative path kalau `raw` berada di bawah `root`,
// atau `raw` apa adanya kalau tidak match.
std::string stripPrefix(const std::string& raw, const std::string& root) {
    if (root.empty() || raw.empty()) return raw;
    std::string prefix = root;
    if (prefix.back() != '/') prefix += '/';
    if (raw.size() > prefix.size() && raw.rfind(prefix, 0) == 0) {
        return raw.substr(prefix.size());
    }
    return raw;
}

} // namespace

// ---------------------------------------------------------------------------
const char* buildPhaseName(BuildPhase p) {
    switch (p) {
        case BuildPhase::Build: return "BUILD";
        case BuildPhase::Test:  return "TEST";
        case BuildPhase::Run:   return "RUN";
    }
    return "?";
}

// ---------------------------------------------------------------------------
BuildSystemKind BuildRunner::detectBuildSystemKind(const std::string& root) {
    namespace fsl = std::filesystem;
    std::error_code ec;

    if (fsl::exists(fsl::path(root) / "CMakeLists.txt", ec) && !ec) {
        return BuildSystemKind::CMake;
    }
    if (fsl::exists(fsl::path(root) / "meson.build", ec) && !ec) {
        return BuildSystemKind::Meson;
    }
    if ((fsl::exists(fsl::path(root) / "Makefile", ec) && !ec)
        || (fsl::exists(fsl::path(root) / "GNUmakefile", ec) && !ec)) {
        return BuildSystemKind::Make;
    }
    if (fsl::exists(fsl::path(root) / "package.json", ec) && !ec) {
        return BuildSystemKind::Npm;
    }
    if (fsl::exists(fsl::path(root) / "pyproject.toml", ec) && !ec) {
        return BuildSystemKind::Poetry;
    }
    return BuildSystemKind::Standalone;
}

// ---------------------------------------------------------------------------
std::unique_ptr<IBuildSystem> BuildRunner::makeBuildSystem(
    BuildSystemKind kind)
{
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

// ---------------------------------------------------------------------------
std::string BuildRunner::formatArgv(const std::vector<std::string>& argv) {
    std::string s;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i) s += ' ';
        s += argv[i];
    }
    return s;
}

// ---------------------------------------------------------------------------
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
        result.success = true;   // skip = tidak menghalangi phase berikutnya
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

        // Safety check per command.
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
            pc.argv,
            eff_timeout,
            /*stdin_data=*/"",
            /*working_dir=*/cwd,
            /*cancel_flag=*/nullptr);

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
            break;   // stop on first failing command in a phase
        }

        cr.success = true;
        result.commands.push_back(std::move(cr));
    }

    const auto t1 = std::chrono::steady_clock::now();
    result.duration_sec = std::chrono::duration<double>(t1 - t0).count();

    // Parse error dari stderr kalau gagal.
    if (!result.success) {
        BuildSystemKind kind = info.kind;
        auto sys = makeBuildSystem(kind);
        if (sys) {
            sys->parseOutput(result, info);
        }
        if (result.error_message.empty()) {
            result.error_message = "Phase " + std::string(buildPhaseName(phase))
                                 + " gagal (exit code != 0).";
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
BuildReport BuildRunner::runBuild(
    const std::vector<GeneratedFile>& files_written,
    const std::string& sandbox_root,
    frans::domain::IFileSystemPort& fs,
    ProjectGraphCache* graph_cache,
    int build_timeout_sec,
    int test_timeout_sec,
    int run_timeout_sec,
    ConfirmCallback confirm_cb)
{
    BuildReport report;
    report.sandbox_root = sandbox_root;

    if (files_written.empty()) {
        report.overall_success = false;
        report.failed_phase    = BuildPhase::Build;
        report.build.skipped   = false;
        report.build.success   = false;
        report.build.error_message = "Tidak ada file untuk di-build.";
        return report;
    }

    // ---- Step 1-3: deteksi project root + build system + test runner ----
    const ProjectInfo info =
        ProjectDetector::detect(files_written, sandbox_root);

    report.project_root     = info.project_root;
    report.build_system     = buildSystemKindName(info.kind);
    report.test_runner      = info.test_runner;
    report.primary_language = info.primary_language;

    std::fprintf(stderr,
        "%s runBuild: project_root=%s build_system=%s test_runner=%s\n",
        kLogPrefix,
        info.project_root.c_str(), report.build_system.c_str(),
        report.test_runner.empty() ? "(none)" : report.test_runner.c_str());
    std::fflush(stderr);

    auto system = makeBuildSystem(info.kind);
    if (!system) {
        report.overall_success = false;
        report.failed_phase    = BuildPhase::Build;
        report.build.skipped   = false;
        report.build.success   = false;
        report.build.error_message = "Internal: build system tidak tersedia.";
        return report;
    }

    // ---- Cek bahwa project_root ada dan di bawah sandbox_root. ----
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

    // ---- Step 4: Build phase ----
    const auto build_cmds = system->buildCommands(info, files_written, fs);
    report.build = runPhase(
        BuildPhase::Build, build_cmds, info.project_root,
        build_timeout_sec, fs, info, confirm_cb);

    if (!report.build.success) {
        report.overall_success = false;
        report.failed_phase    = BuildPhase::Build;
        if (graph_cache && !report.build.related_files.empty()) {
            enrichRelatedFiles(report.build, info.project_root,
                               info.sandbox_root, graph_cache);
        }
        return report;
    }

    // ---- Step 5: Test phase (opsional) ----
    // Skip kalau: tidak ada test runner ATAU tidak ada command test.
    if (!info.test_runner.empty()) {
        const auto test_cmds = system->testCommands(info, fs);
        report.test = runPhase(
            BuildPhase::Test, test_cmds, info.project_root,
            test_timeout_sec, fs, info, confirm_cb);

        if (!report.test.success && !report.test.skipped) {
            report.overall_success = false;
            report.failed_phase    = BuildPhase::Test;
            if (graph_cache && !report.test.related_files.empty()) {
                enrichRelatedFiles(report.test, info.project_root,
                                   info.sandbox_root, graph_cache);
            }
            return report;
        }
    } else {
        // Set phase supaya label UI benar ([TEST], bukan default [BUILD]).
        report.test.phase   = BuildPhase::Test;
        report.test.skipped = true;
        report.test.success = true;
    }

    // ---- Step 6: Run phase (opsional) ----
    {
        const auto run_cmds = system->runCommands(info, files_written);
        report.run = runPhase(
            BuildPhase::Run, run_cmds, info.project_root,
            run_timeout_sec, fs, info, confirm_cb);

        if (!report.run.success && !report.run.skipped) {
            report.overall_success = false;
            report.failed_phase    = BuildPhase::Run;
            if (graph_cache && !report.run.related_files.empty()) {
                enrichRelatedFiles(report.run, info.project_root,
                                   info.sandbox_root, graph_cache);
            }
            return report;
        }
    }

    report.overall_success = true;
    report.total_duration_sec =
        report.build.duration_sec + report.test.duration_sec
        + report.run.duration_sec;

    std::fprintf(stderr,
        "%s runBuild SUCCESS: build=%.3fs test=%.3fs run=%.3fs\n",
        kLogPrefix,
        report.build.duration_sec, report.test.duration_sec,
        report.run.duration_sec);
    std::fflush(stderr);

    return report;
}

// ---------------------------------------------------------------------------
void BuildRunner::enrichRelatedFiles(
    PhaseResult& result,
    const std::string& project_root,
    const std::string& sandbox_root,
    ProjectGraphCache* graph_cache)
{
    if (!graph_cache) return;

    std::string error_file;
    for (const auto& rf : result.related_files) {
        if (rf.role == "PRIMARY_ERROR") { error_file = rf.path; break; }
    }
    if (error_file.empty()) return;

    std::string abs_err = error_file;
    if (!error_file.empty() && error_file[0] != '/') {
        abs_err = project_root + "/" + error_file;
    }

    std::fprintf(stderr,
        "%s   enrichment: query graph untuk '%s'\n",
        kLogPrefix, error_file.c_str());
    std::fflush(stderr);

    const auto& graph = graph_cache->getOrBuild(sandbox_root);

    std::set<std::string> forward;
    std::set<std::string> reverse;

    for (const auto& [sym_name, _occ] : graph.allSymbols()) {
        const auto snippets = graph.findBySymbol(sym_name);
        const auto related  = graph.getRelatedSymbols(sym_name, 1);
        if (related.empty()) continue;

        for (const auto& snip : snippets) {
            for (const auto& rel : related) {
                if (rel.file_path.empty()) continue;
                const std::string snip_rel =
                    stripPrefix(snip.file_path, sandbox_root);
                const std::string rel_rel =
                    stripPrefix(rel.file_path, sandbox_root);

                if (snip_rel == error_file && rel_rel != error_file) {
                    forward.insert(rel_rel);
                }
                if (snip_rel != error_file && rel_rel == error_file) {
                    reverse.insert(snip_rel);
                }
            }
        }
    }

    std::set<std::string> existing;
    for (const auto& rf : result.related_files) existing.insert(rf.path);

    for (const auto& f : reverse) {
        if (existing.count(f)) continue;
        RelatedFile rf;
        rf.path = f;
        rf.role = "IMPORTS";
        rf.note = "imports " + error_file;
        result.related_files.push_back(std::move(rf));
        existing.insert(f);
    }
    for (const auto& f : forward) {
        if (existing.count(f)) continue;
        RelatedFile rf;
        rf.path = f;
        rf.role = "DEPENDENT";
        rf.note = "referenced by " + error_file;
        result.related_files.push_back(std::move(rf));
        existing.insert(f);
    }
}

} // namespace frans::application
