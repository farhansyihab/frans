// ~/AgenAI/frans/src/application/agent/ExecuteState.cpp
#include "application/agent/ExecuteState.h"

#include "application/agent/AgentContext.h"
#include "application/safety/DangerCheckHandler.h"
#include "domain/fs/IFileSystemPort.h"
#include "infrastructure/system/ProcessExecutor.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace frans::application {

namespace {

namespace fs = std::filesystem;

// Tentukan interpreter + argv dari ekstensi file.
// Return false kalau bahasa tidak didukung (Stage 7c: hanya .py dan .sh).
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

// Ringkas output kegagalan menjadi satu baris human-readable untuk
// ctx.lastError. Prioritas: stderr (biasanya error trace); kalau kosong,
// stdout; kalau dua-duanya kosong, cukup "exit_code=N".
std::string summarizeFailure(int exit_code, bool timed_out,
                             const std::string& stdout_text,
                             const std::string& stderr_text) {
    std::string head = timed_out
        ? std::string("Eksekusi TIMEOUT.")
        : ("Eksekusi gagal (exit_code=" + std::to_string(exit_code) + ").");

    const std::string& src = !stderr_text.empty() ? stderr_text : stdout_text;
    if (src.empty()) return head;

    // Ambil tail 400 char — pesan error Python biasanya di baris akhir.
    constexpr std::size_t kMax = 400;
    if (src.size() <= kMax) {
        head += " Output: " + src;
    } else {
        head += " Output (tail): " + src.substr(src.size() - kMax);
    }
    return head;
}

} // namespace

ExecuteState::ExecuteState(std::string                     sandbox_root,
                           frans::domain::IFileSystemPort& fs,
                           ConfirmCallback                 confirm,
                           int                             default_timeout_sec)
    : sandbox_root_(std::move(sandbox_root)),
      fs_(fs),
      confirm_(std::move(confirm)),
      default_timeout_sec_(default_timeout_sec > 0 ? default_timeout_sec : 30) {}

void ExecuteState::run(AgentContext& ctx) {
    // Reset status supaya caller tidak membaca hasil run sebelumnya.
    ctx.success = false;
    ctx.lastError.clear();
    ctx.lastExecution.reset();

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

    // ---- 2. Baca isi file ----
    const std::string code = fs_.readFile(ctx.targetRelativePath);
    if (code.empty()) {
        ctx.lastError = "ExecuteState: file target kosong.";
        return;
    }

    // ---- 3. Bangun absolute path ----
    const fs::path abs_path =
        fs::path(sandbox_root_) / ctx.targetRelativePath;

    // ---- 4. Danger check ----
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
            // Konservatif: tanpa callback, tolak.
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

    // ---- 5. Build argv ----
    std::vector<std::string> argv;
    if (!buildArgvForScript(abs_path.string(), argv)) {
        ctx.lastError = "ExecuteState: ekstensi file tidak didukung: "
                      + abs_path.extension().string();
        return;
    }

    // ---- 6. Run ----
    const auto t0 = std::chrono::steady_clock::now();
    const auto proc = frans::infrastructure::runCommand(
        argv,
        default_timeout_sec_,
        /*stdin_data=*/"",
        /*working_dir=*/sandbox_root_);
    const auto t1 = std::chrono::steady_clock::now();

    // ---- 7. Simpan hasil ----
    // `ran = true` menandakan runCommand BENAR-BENAR dipanggil (bukan
    // dipotong di langkah 1-5). ObserveState memakai flag ini untuk
    // membedakan "tidak jalan" dari "jalan tapi gagal".
    ctx.lastExecution.ran          = true;
    ctx.lastExecution.exit_code    = proc.exit_code;
    ctx.lastExecution.timed_out    = proc.timed_out;
    ctx.lastExecution.duration_sec =
        std::chrono::duration<double>(t1 - t0).count();
    ctx.lastExecution.stdout_text  = proc.stdout_text;
    ctx.lastExecution.stderr_text  = proc.stderr_text;

    if (ctx.lastExecution.is_success()) {
        ctx.success = true;
        ctx.lastError.clear();
    } else {
        ctx.success = false;
        ctx.lastError = summarizeFailure(
            proc.exit_code, proc.timed_out,
            proc.stdout_text, proc.stderr_text);
    }
}

} // namespace frans::application
