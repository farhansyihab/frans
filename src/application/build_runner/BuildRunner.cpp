// ~/AgenAI/frans/src/application/build_runner/BuildRunner.cpp
#include "application/build_runner/BuildRunner.h"

#include "application/build_runner/CppBuildStrategy.h"
#include "application/build_runner/JavaScriptBuildStrategy.h"
#include "application/build_runner/PythonBuildStrategy.h"
#include "application/build_runner/TypeScriptBuildStrategy.h"
#include "application/project/ProjectGraphCache.h"
#include "application/safety/DangerCheckHandler.h"
#include "code_intelligence/graph/DependencyGraph.h"
#include "infrastructure/system/ProcessExecutor.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace frans::application {

namespace {

constexpr const char* kLogPrefix = "[frans][build_runner]";

bool isCppExt(const std::string& ext) {
    return ext == ".cpp" || ext == ".cc" || ext == ".cxx"
        || ext == ".hpp" || ext == ".hxx" || ext == ".hh" || ext == ".h";
}

} // namespace

// ---------------------------------------------------------------------------
// detectDominantLanguage
// ---------------------------------------------------------------------------
BuildLanguage BuildRunner::detectDominantLanguage(
    const std::vector<GeneratedFile>& files)
{
    std::map<BuildLanguage, int> counts;

    for (const auto& f : files) {
        const auto dot = f.relative_path.find_last_of('.');
        if (dot == std::string::npos) continue;
        const std::string ext = f.relative_path.substr(dot);

        if (ext == ".py" || ext == ".pyw" || ext == ".pyi") {
            counts[BuildLanguage::Python]++;
        } else if (isCppExt(ext)) {
            counts[BuildLanguage::Cpp]++;
        } else if (ext == ".ts" || ext == ".tsx") {
            counts[BuildLanguage::TypeScript]++;
        } else if (ext == ".js" || ext == ".jsx"
                || ext == ".mjs" || ext == ".cjs") {
            counts[BuildLanguage::JavaScript]++;
        } else if (ext == ".vue") {
            counts[BuildLanguage::Vue]++;
        }
    }

    if (counts.empty()) return BuildLanguage::Unknown;

    auto best = std::max_element(
        counts.begin(), counts.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    return best->first;
}

// ---------------------------------------------------------------------------
// languageName
// ---------------------------------------------------------------------------
const char* BuildRunner::languageName(BuildLanguage lang) {
    switch (lang) {
        case BuildLanguage::Python:      return "Python";
        case BuildLanguage::Cpp:         return "C++";
        case BuildLanguage::TypeScript:  return "TypeScript";
        case BuildLanguage::JavaScript:  return "JavaScript";
        case BuildLanguage::Vue:         return "Vue";
        case BuildLanguage::Unknown:     return "Unknown";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// makeStrategy
// ---------------------------------------------------------------------------
std::unique_ptr<IBuildStrategy> BuildRunner::makeStrategy(
    BuildLanguage lang)
{
    switch (lang) {
        case BuildLanguage::Python:
            return std::make_unique<PythonBuildStrategy>();
        case BuildLanguage::Cpp:
            return std::make_unique<CppBuildStrategy>();
        case BuildLanguage::TypeScript:
            return std::make_unique<TypeScriptBuildStrategy>();
        case BuildLanguage::JavaScript:
            return std::make_unique<JavaScriptBuildStrategy>();
        case BuildLanguage::Vue:
        case BuildLanguage::Unknown:
            return nullptr;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// formatArgv
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
// runBuild
// ---------------------------------------------------------------------------
BuildReport BuildRunner::runBuild(
    const std::vector<GeneratedFile>& files_written,
    const std::string& sandbox_root,
    frans::domain::IFileSystemPort& fs,
    ProjectGraphCache* graph_cache,
    int timeout_sec,
    ConfirmCallback confirm_cb)
{
    BuildReport report;

    if (files_written.empty()) {
        report.success       = false;
        report.error_message = "Tidak ada file untuk di-build.";
        std::fprintf(stderr, "%s runBuild: files_written kosong\n",
                     kLogPrefix);
        std::fflush(stderr);
        return report;
    }

    const BuildLanguage lang = detectDominantLanguage(files_written);
    report.language = languageName(lang);

    std::fprintf(stderr,
        "%s runBuild: detected=%s files=%zu sandbox=%s\n",
        kLogPrefix, report.language.c_str(), files_written.size(),
        sandbox_root.c_str());
    std::fflush(stderr);

    if (lang == BuildLanguage::Vue) {
        report.success = false;
        report.error_message =
            "Vue SFC tidak bisa di-build standalone. Jalankan via dev server "
            "(mis. `npm run dev`) di terminal eksternal.";
        return report;
    }
    if (lang == BuildLanguage::Unknown) {
        report.success = false;
        report.error_message =
            "Bahasa tidak dikenali dari ekstensi file yang diimpor. "
            "Build/Debug hanya mendukung: .py, .cpp/.cc/.cxx, .ts/.tsx, "
            ".js/.jsx/.mjs/.cjs.";
        return report;
    }

    auto strategy = makeStrategy(lang);
    if (!strategy) {
        report.success = false;
        report.error_message = "Internal: strategy tidak tersedia untuk "
                              + report.language;
        return report;
    }

    const std::string entry = strategy->detectEntryPoint(
        files_written, fs, sandbox_root);
    if (entry.empty()) {
        report.success = false;
        report.error_message = "Entry point tidak terdeteksi untuk "
                              + report.language
                              + ". Coba import file dengan fungsi main.";
        return report;
    }
    report.entry_point = entry;

    std::fprintf(stderr, "%s   entry point: %s\n",
                 kLogPrefix, entry.c_str());
    std::fflush(stderr);

    // Stage 11d follow-up — pass sandbox_root ke buildCommand supaya
    // CppBuildStrategy bisa tambah include path.
    std::vector<std::string> argv = strategy->buildCommand(
        entry, files_written, sandbox_root);

    if (argv.empty()) {
        report.success = false;
        report.error_message = "Gagal membangun build command untuk "
                              + report.language;
        return report;
    }
    report.command = formatArgv(argv);

    // Deteksi mode syntax-only dari argv (di-set oleh CppBuildStrategy
    // kalau tidak ada `int main` di file yang diimpor). Dipakai UI untuk
    // menampilkan catatan khusus.
    bool syntax_only_mode = false;
    for (const auto& a : argv) {
        if (a == "-fsyntax-only") { syntax_only_mode = true; break; }
    }

    std::fprintf(stderr, "%s   command: %s\n",
                 kLogPrefix, report.command.c_str());
    if (syntax_only_mode) {
        std::fprintf(stderr,
            "%s   mode: SYNTAX CHECK ONLY (tidak ada `int main`)\n",
            kLogPrefix);
    }
    std::fflush(stderr);

    // ---- Danger check ----
    DangerCheckHandler danger;
    const DangerCheckResult dr = danger.checkCommand(argv);

    if (dr.is_blocked()) {
        report.success = false;
        report.error_message = "Command DIBLOKIR safety layer: "
                              + dr.explanation;
        std::fprintf(stderr, "%s   BLOCKED by danger check: %s\n",
                     kLogPrefix, dr.explanation.c_str());
        std::fflush(stderr);
        return report;
    }
    if (dr.needs_confirm()) {
        if (!confirm_cb) {
            report.success = false;
            report.error_message =
                "Command butuh konfirmasi user, tapi tidak ada callback "
                "yang diset. Command ditolak. Pattern: " + dr.explanation;
            std::fprintf(stderr,
                "%s   NEEDS-CONFIRM tapi confirm_cb kosong → ditolak\n",
                kLogPrefix);
            std::fflush(stderr);
            return report;
        }
        const std::string reason =
            "Build command akan dijalankan:\n\n  " + report.command
            + "\n\nPattern berisiko terdeteksi: " + dr.explanation
            + "\n\nLanjutkan?";
        if (!confirm_cb(reason)) {
            report.success = false;
            report.error_message = "User menolak eksekusi build command.";
            std::fprintf(stderr,
                "%s   user menolak konfirmasi build command\n",
                kLogPrefix);
            std::fflush(stderr);
            return report;
        }
    }

    // ---- Run ----
    const int effective_timeout = timeout_sec > 0 ? timeout_sec : 30;

    std::fprintf(stderr,
        "%s   running (timeout=%ds, cwd=%s)...\n",
        kLogPrefix, effective_timeout, sandbox_root.c_str());
    std::fflush(stderr);

    const auto proc = frans::infrastructure::runCommand(
        argv,
        effective_timeout,
        /*stdin_data=*/"",
        /*working_dir=*/sandbox_root,
        /*cancel_flag=*/nullptr);

    std::fprintf(stderr,
        "%s   subprocess done: exit=%d timed_out=%d stdout=%zu stderr=%zu\n",
        kLogPrefix, proc.exit_code, proc.timed_out ? 1 : 0,
        proc.stdout_text.size(), proc.stderr_text.size());
    std::fflush(stderr);

    // ---- Parse output ----
    report = strategy->parseOutput(
        proc.exit_code,
        proc.timed_out,
        proc.stdout_text,
        proc.stderr_text,
        /*duration_sec=*/0.0,
        sandbox_root);

    // Re-set field yang tidak diisi strategy.
    report.language    = strategy->languageName();
    report.entry_point = entry;
    report.command     = formatArgv(argv);
    report.syntax_only = syntax_only_mode;

    // ---- Enrichment ----
    if (!report.success && graph_cache) {
        enrichRelatedFiles(report, sandbox_root, graph_cache);
    }

    return report;
}

// ---------------------------------------------------------------------------
// enrichRelatedFiles
// ---------------------------------------------------------------------------
void BuildRunner::enrichRelatedFiles(
    BuildReport& report,
    const std::string& sandbox_root,
    ProjectGraphCache* graph_cache)
{
    if (!graph_cache) return;

    std::string error_file;
    for (const auto& rf : report.related_files) {
        if (rf.role == "PRIMARY_ERROR") {
            error_file = rf.path;
            break;
        }
    }
    if (error_file.empty()) {
        std::fprintf(stderr,
            "%s   enrichment skip: tidak ada PRIMARY_ERROR\n",
            kLogPrefix);
        std::fflush(stderr);
        return;
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
        const auto related  = graph.getRelatedSymbols(sym_name, /*maxHops=*/1);
        if (related.empty()) continue;

        for (const auto& snip : snippets) {
            for (const auto& rel : related) {
                if (rel.file_path.empty()) continue;

                if (snip.file_path == error_file
                    && rel.file_path != error_file)
                {
                    forward.insert(rel.file_path);
                }
                if (snip.file_path != error_file
                    && rel.file_path == error_file
                    && !snip.file_path.empty())
                {
                    reverse.insert(snip.file_path);
                }
            }
        }
    }

    std::fprintf(stderr,
        "%s   enrichment: forward=%zu reverse=%zu\n",
        kLogPrefix, forward.size(), reverse.size());
    std::fflush(stderr);

    std::set<std::string> existing;
    for (const auto& rf : report.related_files) existing.insert(rf.path);

    for (const auto& f : reverse) {
        if (existing.count(f)) continue;
        RelatedFile rf;
        rf.path = f;
        rf.role = "IMPORTS";
        rf.note = "imports " + error_file;
        report.related_files.push_back(std::move(rf));
        existing.insert(f);
    }
    for (const auto& f : forward) {
        if (existing.count(f)) continue;
        RelatedFile rf;
        rf.path = f;
        rf.role = "DEPENDENT";
        rf.note = "referenced by " + error_file;
        report.related_files.push_back(std::move(rf));
        existing.insert(f);
    }
}

} // namespace frans::application