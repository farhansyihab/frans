// ~/AgenAI/frans/src/application/build_runner/BuildRunner.h
#pragma once

#include "application/agent/MultiFileParser.h"      // GeneratedFile
#include "application/build_runner/BuildReport.h"
#include "application/build_runner/IBuildStrategy.h"
#include "domain/fs/IFileSystemPort.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace frans::application {

// Forward decl — hindari include ProjectGraphCache.h (yang transitively
// include DependencyGraph.h → tree-sitter). Caller yang butuh enrichment
// menyediakan pointer ke instance nyata; kalau nullptr, enrichment di-skip.
class ProjectGraphCache;

// Bahasa yang terdeteksi dari ekstensi file. Dipakai di
// BuildRunner::detectDominantLanguage().
enum class BuildLanguage {
    Python,
    Cpp,
    TypeScript,
    JavaScript,
    Vue,
    Unknown,
};

// Blueprint Stage 11d §13.3 — Facade untuk build/compile/run file yang
// baru diimpor via Manual Import.
//
// Alur (tiap panggilan runBuild):
//   1. Deteksi bahasa dominan dari ekstensi file yang ditulis.
//   2. Pilih IBuildStrategy yang sesuai (Vue → error informatif).
//   3. Strategy::detectEntryPoint(files) → entry point.
//   4. Strategy::buildCommand(entry, files) → argv.
//   5. DangerCheckHandler.checkCommand(argv) → safety check.
//      - Safe       → lanjut.
//      - NeedsConfirm → panggil confirm_cb (kalau ada); tolak kalau tidak.
//      - Blocked    → return BuildReport dengan error.
//   6. ProcessExecutor::runCommand(argv, timeout, cwd=sandbox_root).
//   7. Strategy::parseOutput(stdout, stderr) → BuildReport.
//   8. Kalau ada error DAN graph_cache != nullptr:
//        - Enrich related_files via DependencyGraph.
//   9. Return BuildReport.
//
// THREADING:
//   Method ini BLOCKING (memanggil ProcessExecutor::runCommand yang
//   menunggu subprocess). Blueprint §13.3 menyebut boleh dipanggil dari
//   UI thread — untuk konsistensi dengan doParse()/doWrite() di
//   ManualImportDialog, kita memanggilnya sinkron dari UI. Build command
//   dibatasi timeout 30 detik (default), jadi worst-case UI freeze
//   ~30 detik. Untuk build yang lebih berat, user bisa naikkan timeout
//   lewat parameter, tapi tidak ada mekanisme async — itu future work.
//
// Thread-safety:
//   Tidak ada state internal yang di-mutate. Boleh dipanggil dari thread
//   mana pun. `graph_cache` HARUS tetap valid selama panggilan — kalau
//   ada thread lain yang bisa meng-invalidate/meng-rebuild graph,
//   caller yang bertanggung jawab serialize akses ke graph_cache.
class BuildRunner {
public:
    // Callback konfirmasi user untuk command NeedsConfirm.
    // Return true = lanjut, false = tolak.
    // Kalau nullptr DAN verdict == NeedsConfirm → command DITOLAK
    // (safe default).
    using ConfirmCallback = std::function<bool(const std::string& reason)>;

    BuildRunner() = default;

    BuildRunner(const BuildRunner&)            = delete;
    BuildRunner& operator=(const BuildRunner&) = delete;

    // Jalankan build untuk file-file yang baru ditulis.
    //
    // `files_written`    : list file yang di-commit via ManualImporter.
    // `sandbox_root`     : absolute path sandbox root (canonical).
    // `fs`               : IFileSystemPort — dipakai strategy kalau perlu
    //                      baca file di luar `files_written`.
    // `graph_cache`      : ProjectGraphCache untuk enrich related files.
    //                      Boleh nullptr → related_files tidak di-enrich.
    // `timeout_sec`      : timeout subprocess (default 30).
    // `confirm_cb`       : callback konfirmasi untuk command NeedsConfirm.
    //
    // Return BuildReport dengan field terisi. Tidak throw.
    BuildReport runBuild(
        const std::vector<GeneratedFile>& files_written,
        const std::string& sandbox_root,
        frans::domain::IFileSystemPort& fs,
        ProjectGraphCache* graph_cache,
        int timeout_sec = 30,
        ConfirmCallback confirm_cb = nullptr);

    // Deteksi bahasa dominan dari list file (static, murni).
    // Return Unknown kalau list kosong atau tidak ada ekstensi dikenali.
    static BuildLanguage detectDominantLanguage(
        const std::vector<GeneratedFile>& files);

    // Utility: nama string untuk BuildLanguage (untuk log/UI).
    static const char* languageName(BuildLanguage lang);

private:
    // Factory: pilih strategy berdasarkan bahasa. Return nullptr untuk
    // bahasa yang tidak punya strategy (Vue, Unknown).
    static std::unique_ptr<IBuildStrategy> makeStrategy(BuildLanguage lang);

    // Enrich report.related_files via DependencyGraph.
    // Dipanggil HANYA kalau report.success == false DAN ada PRIMARY_ERROR.
    void enrichRelatedFiles(
        BuildReport& report,
        const std::string& sandbox_root,
        ProjectGraphCache* graph_cache);

    // Helper: format argv jadi string untuk ditampilkan di BuildReport.command.
    static std::string formatArgv(const std::vector<std::string>& argv);
};

} // namespace frans::application