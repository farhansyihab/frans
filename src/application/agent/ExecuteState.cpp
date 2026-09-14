// ~/AgenAI/frans/src/application/agent/ExecuteState.cpp
#include "application/agent/ExecuteState.h"

#include "application/agent/AgentContext.h"
#include "application/safety/DangerCheckHandler.h"
#include "domain/fs/IFileSystemPort.h"
#include "infrastructure/system/ProcessExecutor.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace frans::application {

namespace {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers untuk entry point (existing)
// ---------------------------------------------------------------------------

bool buildArgvForScript(const std::string& abs_path,
                        std::vector<std::string>& argv_out)
{
    const std::string ext = fs::path(abs_path).extension().string();
    if (ext == ".py") {
        argv_out = {"python3", abs_path};
        return true;
    }
    if (ext == ".sh") {
        argv_out = {"bash", abs_path};
        return true;
    }
    return false;
}

std::string languageFromExtension(const std::string& abs_path) {
    const std::string ext = fs::path(abs_path).extension().string();
    if (ext == ".py") return "python";
    if (ext == ".sh") return "shell";
    return "unknown";
}

std::string summarizeFailure(int exit_code, bool timed_out,
                             const std::string& stdout_text,
                             const std::string& stderr_text) {
    std::string head = timed_out
        ? std::string("Eksekusi TIMEOUT.")
        : ("Eksekusi gagal (exit_code=" + std::to_string(exit_code) + ").");

    const std::string& src = !stderr_text.empty() ? stderr_text : stdout_text;
    if (src.empty()) return head;

    constexpr std::size_t kMax = 400;
    if (src.size() <= kMax) {
        head += " Output: " + src;
    } else {
        head += " Output (tail): " + src.substr(src.size() - kMax);
    }
    return head;
}

// ---------------------------------------------------------------------------
// Stage 7f.5 — helper deteksi test file
// ---------------------------------------------------------------------------

// Cek apakah nama file match konvensi test: "test_*.py" atau "*_test.py".
bool looksLikeTestName(const std::string& filename) {
    if (filename.size() < 4) return false;
    if (filename.compare(filename.size() - 3, 3, ".py") != 0) return false;

    // test_*.py
    if (filename.rfind("test_", 0) == 0) return true;

    // *_test.py
    constexpr std::size_t kSuffixLen = 8;   // "_test.py"
    if (filename.size() > kSuffixLen
        && filename.compare(filename.size() - kSuffixLen, kSuffixLen,
                            "_test.py") == 0) {
        return true;
    }
    return false;
}

// Cek isi file mengandung "import unittest" atau "import pytest".
// Dibaca via IFileSystemPort supaya path tetap di dalam sandbox.
bool containsTestImport(frans::domain::IFileSystemPort& fs,
                        const std::string& rel_path) {
    const std::string content = fs.readFile(rel_path);
    if (content.empty()) return false;
    return content.find("import unittest") != std::string::npos
        || content.find("import pytest")   != std::string::npos;
}

// Scan TOP-LEVEL sandbox_root (non-recursive) untuk test file.
// Return path RELATIF terhadap sandbox_root, atau "" kalau tidak ada.
//
// Alasan top-level saja: subdirectory butuh penanganan sys.path / package
// yang lebih kompleks (test_x.py di tests/ yang import module dari root
// akan gagal). MVP 7f.5 fokus ke kasus paling umum. Kalau nanti butuh
// recursive, extend dengan `python3 -m unittest discover`.
//
// Kalau ada beberapa kandidat, ambil yang PERTAMA secara alfabetis
// (deterministik). Multi-test-file aggregation adalah future work.
std::string findTopLevelTestFile(const std::string& sandbox_root,
                                 frans::domain::IFileSystemPort& fs) {
    std::error_code ec;

    if (!fs::exists(sandbox_root, ec)
        || !fs::is_directory(sandbox_root, ec)) {
        return {};
    }

    std::vector<std::string> candidates;

    for (auto it = fs::directory_iterator(sandbox_root, ec);
         it != fs::directory_iterator();
         it.increment(ec))
    {
        if (ec) break;
        const auto& p = it->path();

        if (fs::is_directory(p, ec)) continue;

        const std::string fname = p.filename().string();
        if (!looksLikeTestName(fname)) continue;
        if (!containsTestImport(fs, fname)) continue;

        candidates.push_back(fname);
    }

    if (candidates.empty()) return {};
    std::sort(candidates.begin(), candidates.end());
    return candidates.front();
}

// Strip ".py" dari filename → module name (untuk `-m unittest`).
std::string moduleNameFromFile(const std::string& filename) {
    if (filename.size() > 3
        && filename.compare(filename.size() - 3, 3, ".py") == 0) {
        return filename.substr(0, filename.size() - 3);
    }
    return filename;
}

} // namespace

// ---------------------------------------------------------------------------
// ExecuteState
// ---------------------------------------------------------------------------

ExecuteState::ExecuteState(std::string                     sandbox_root,
                           frans::domain::IFileSystemPort& fs,
                           ConfirmCallback                 confirm,
                           int                             default_timeout_sec,
                           std::atomic<bool>*              cancel_flag)
    : sandbox_root_(std::move(sandbox_root)),
      fs_(fs),
      confirm_(std::move(confirm)),
      default_timeout_sec_(default_timeout_sec > 0 ? default_timeout_sec : 30),
      cancel_flag_(cancel_flag) {}

void ExecuteState::run(AgentContext& ctx) {
    // Reset status supaya caller tidak membaca hasil run sebelumnya.
    ctx.success = false;
    ctx.lastError.clear();
    ctx.lastExecution.reset();
    ctx.lastTest.reset();

    // ---- 1. Validasi target ----
    if (ctx.targetRelativePath.empty()) {
        ctx.lastError = "ExecuteState: targetRelativePath kosong.";
        return;
    }
    if (!fs_.exists(ctx.targetRelativePath)) {
        ctx.lastError = "ExecuteState: file target tidak ada di sandbox: "
                      + ctx.targetRelativePath;
        return;
    }

    // ---- 2. Baca isi file entry point ----
    const std::string code = fs_.readFile(ctx.targetRelativePath);
    if (code.empty()) {
        ctx.lastError = "ExecuteState: file target kosong.";
        return;
    }

    // ---- 3. Bangun absolute path entry point ----
    const fs::path abs_path =
        fs::path(sandbox_root_) / ctx.targetRelativePath;

    // ---- 4. Danger check entry point ----
    DangerCheckHandler danger;
    const std::string lang = languageFromExtension(abs_path.string());
    const DangerCheckResult dr = danger.checkCode(code, lang);

    if (dr.is_blocked()) {
        ctx.lastError = "ExecuteState: BLOCKED oleh DangerCheckHandler. "
                      + dr.explanation;
        return;
    }
    if (dr.needs_confirm()) {
        const std::string reason =
            "Kode yang akan dijalankan mengandung pola berisiko.\n\n"
            + dr.explanation
            + "\n\nLanjut eksekusi?";

        if (!confirm_) {
            ctx.lastError = "ExecuteState: butuh konfirmasi user tapi "
                            "callback tidak diset → ditolak. "
                          + dr.explanation;
            return;
        }
        if (!confirm_(reason)) {
            ctx.lastError = "ExecuteState: user menolak eksekusi. "
                          + dr.explanation;
            return;
        }
    }

    // =====================================================================
    // 5. Stage 7f.5 — test-aware execution
    //
    // Kalau ada test file di top-level sandbox (dan != entry point),
    // jalankan test DULU. Test pass → lanjut entry. Test fail → STOP.
    // =====================================================================
    {
        std::string test_file = findTopLevelTestFile(sandbox_root_, fs_);

        // Skip kalau test file = entry point (user berniat menjalankan
        // file test sebagai entry point, bukan sebagai test).
        if (test_file == ctx.targetRelativePath) {
            test_file.clear();
        }

        if (!test_file.empty()) {
            ctx.lastTest.ran       = true;
            ctx.lastTest.test_file = test_file;

            // ---- 5a. Danger check test file ----
            const std::string test_code = fs_.readFile(test_file);
            const DangerCheckResult dr_test =
                danger.checkCode(test_code, "python");

            if (dr_test.is_blocked()) {
                ctx.lastTest.exit_code = -1;
                ctx.lastError = "ExecuteState: test file '" + test_file
                              + "' BLOCKED oleh DangerCheckHandler. "
                              + dr_test.explanation;
                return;
            }
            if (dr_test.needs_confirm()) {
                const std::string reason =
                    "Test file '" + test_file
                    + "' mengandung pola berisiko.\n\n"
                    + dr_test.explanation
                    + "\n\nLanjut eksekusi test?";

                if (!confirm_) {
                    ctx.lastTest.exit_code = -1;
                    ctx.lastError = "ExecuteState: test butuh konfirmasi "
                                    "tapi callback tidak diset → ditolak. "
                                  + dr_test.explanation;
                    return;
                }
                if (!confirm_(reason)) {
                    ctx.lastTest.exit_code = -1;
                    ctx.lastError = "ExecuteState: user menolak eksekusi test.";
                    return;
                }
            }

            // ---- 5b. Jalankan test ----
            const std::string module_name = moduleNameFromFile(test_file);

            const std::vector<std::string> test_argv = {
                "python3", "-m", "unittest", module_name
            };

            std::fprintf(stderr,
                "[frans][execute] test-aware: found '%s', "
                "running 'python3 -m unittest %s' (cwd=%s)\n",
                test_file.c_str(), module_name.c_str(),
                sandbox_root_.c_str());
            std::fflush(stderr);

            const auto t0 = std::chrono::steady_clock::now();
            const auto proc = frans::infrastructure::runCommand(
                test_argv,
                default_timeout_sec_,
                /*stdin_data=*/"",
                /*working_dir=*/sandbox_root_,
                /*cancel_flag=*/cancel_flag_);
            const auto t1 = std::chrono::steady_clock::now();

            ctx.lastTest.exit_code    = proc.exit_code;
            ctx.lastTest.timed_out    = proc.timed_out;
            ctx.lastTest.duration_sec =
                std::chrono::duration<double>(t1 - t0).count();

            std::fprintf(stderr,
                "[frans][execute] test %s (exit=%d, %.2fs)%s\n",
                ctx.lastTest.is_success() ? "PASS" : "FAIL",
                proc.exit_code,
                ctx.lastTest.duration_sec,
                ctx.lastTest.is_success() ? " → lanjut entry point"
                                          : " → skip entry point");
            std::fflush(stderr);

            // ---- 5c. Test fail → jangan jalankan entry point ----
            if (!ctx.lastTest.is_success()) {
                // Salin hasil test ke lastExecution supaya ObserveState &
                // FixState dapat sinyal error yang benar (tanpa tahu bahwa
                // ini sebenarnya output test, bukan output entry point).
                ctx.lastExecution.ran               = true;
                ctx.lastExecution.exit_code         = proc.exit_code;
                ctx.lastExecution.timed_out         = proc.timed_out;
                ctx.lastExecution.cancelled_by_user = proc.cancelled_by_user;
                ctx.lastExecution.duration_sec      = ctx.lastTest.duration_sec;
                ctx.lastExecution.stdout_text       = proc.stdout_text;
                ctx.lastExecution.stderr_text       = proc.stderr_text;

                ctx.success = false;
                if (proc.cancelled_by_user) {
                    ctx.lastError = "Test cancelled by user in " + test_file;
                } else if (proc.timed_out) {
                    ctx.lastError = "Test TIMEOUT in " + test_file;
                } else {
                    ctx.lastError = "Test failed (exit="
                                  + std::to_string(proc.exit_code)
                                  + ") in " + test_file;
                }
                return;
            }
            // Test pass → jatuh ke langkah 6 (jalankan entry point).
        }
    }

    // ---- 6. Build argv untuk entry point ----
    std::vector<std::string> argv;
    if (!buildArgvForScript(abs_path.string(), argv)) {
        ctx.lastError = "ExecuteState: ekstensi file tidak didukung: "
                      + abs_path.extension().string();
        return;
    }

    // ---- 7. Run entry point ----
    const auto t0 = std::chrono::steady_clock::now();
    const auto proc = frans::infrastructure::runCommand(
        argv,
        default_timeout_sec_,
        /*stdin_data=*/"",
        /*working_dir=*/sandbox_root_,
        /*cancel_flag=*/cancel_flag_);
    const auto t1 = std::chrono::steady_clock::now();

    // ---- 8. Simpan hasil ----
    ctx.lastExecution.ran               = true;
    ctx.lastExecution.exit_code         = proc.exit_code;
    ctx.lastExecution.timed_out         = proc.timed_out;
    ctx.lastExecution.cancelled_by_user = proc.cancelled_by_user;
    ctx.lastExecution.duration_sec =
        std::chrono::duration<double>(t1 - t0).count();
    ctx.lastExecution.stdout_text  = proc.stdout_text;
    ctx.lastExecution.stderr_text  = proc.stderr_text;

    if (ctx.lastExecution.is_success()) {
        ctx.success = true;
        ctx.lastError.clear();
    } else {
        ctx.success = false;
        if (proc.cancelled_by_user) {
            ctx.lastError = "Execution cancelled by user.";
        } else {
            ctx.lastError = summarizeFailure(
                proc.exit_code, proc.timed_out,
                proc.stdout_text, proc.stderr_text);
        }
    }
}

} // namespace frans::application
