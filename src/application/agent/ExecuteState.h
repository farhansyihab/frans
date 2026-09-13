// ~/AgenAI/frans/src/application/agent/ExecuteState.h
#pragma once

#include "application/agent/IAgentState.h"

#include <functional>
#include <string>

namespace frans::domain {
class IFileSystemPort;
}

namespace frans::application {

// Blueprint §5.7 — ExecuteState (Stage 7c).
//
// Satu langkah: jalankan file yang sudah digenerate lewat interpreter
// yang sesuai, tangkap stdout/stderr, catat exit code + durasi.
//
// Alur:
//   1. Validasi targetRelativePath tidak kosong dan file-nya ada di sandbox.
//   2. Baca isi file → tentukan bahasa dari ekstensi (.py, .sh).
//   3. DangerCheckHandler::checkCode() → verdict.
//      - Blocked       → fail, tidak dijalankan.
//      - NeedsConfirm  → panggil confirm_(); user tolak → fail.
//      - Safe          → lanjut.
//   4. Build argv: {"python3", "<abs_path>"} untuk .py.
//   5. runCommand(argv, timeout, "", sandbox_root) — child chdir ke sandbox.
//   6. Simpan hasil ke ctx.lastExecution + update ctx.success/lastError.
//
// Yang TIDAK dilakukan di sini (Stage 7d):
//   - Analisis semantik output (ObserveState).
//   - Retry / eskalasi provider (FixState).
//
// Threading: run() biasanya dipanggil dari worker thread. ConfirmCallback
// dipanggil dari thread yang sama. Implementasi UI WAJIB marshall ke main
// thread lewat mekanisme polling timer (Fl::add_timeout), bukan Fl::awake.
class ExecuteState : public IAgentState {
public:
    // Callback konfirmasi. Return true = user setuju menjalankan.
    // Kalau null dan verdict == NeedsConfirm → konservatif: tolak.
    using ConfirmCallback = std::function<bool(const std::string& reason)>;

    // `sandbox_root` WAJIB absolute canonical (biasanya
    // SandboxedFileSystem::root().string()). Dipakai sebagai cwd child
    // supaya script berjalan di dalam batas sandbox.
    ExecuteState(std::string                     sandbox_root,
                 frans::domain::IFileSystemPort& fs,
                 ConfirmCallback                 confirm             = nullptr,
                 int                             default_timeout_sec = 30);

    std::string name() const override { return "Execute"; }
    void run(AgentContext& ctx) override;

private:
    std::string                     sandbox_root_;
    frans::domain::IFileSystemPort& fs_;
    ConfirmCallback                 confirm_;
    int                             default_timeout_sec_;
};

} // namespace frans::application
