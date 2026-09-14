// ~/AgenAI/frans/src/application/agent/ExecuteState.h
#pragma once

#include "application/agent/IAgentState.h"

#include <atomic>
#include <functional>
#include <string>

namespace frans::domain {
class IFileSystemPort;
}

namespace frans::application {

// Blueprint §5.7 — ExecuteState (Stage 7c).
// Blueprint Stage 7f.5 — Test-aware execution.
// Blueprint Stage 7g — Cancel mid-execute.
//
// Alur (Stage 7f.5 + 7g):
//   1. Validasi targetRelativePath & file-nya ada di sandbox.
//   2. Baca isi file → tentukan bahasa dari ekstensi (.py, .sh).
//   3. DangerCheckHandler::checkCode() → verdict untuk ENTRY POINT.
//   4. Deteksi test file di top-level sandbox:
//        - nama match "test_*.py" atau "*_test.py", DAN
//        - isi mengandung "import unittest" atau "import pytest".
//   5. Kalau test file ada DAN != targetRelativePath:
//        a. Danger check test file juga.
//        b. Jalankan "python3 -m unittest <module_name>" dari sandbox root.
//           Module name = filename tanpa ".py".
//        c. Isi ctx.lastTest (ran, test_file, exit_code, timed_out,
//           duration_sec).
//        d. Kalau test FAIL → STOP, salin hasil test ke ctx.lastExecution
//           (supaya ObserveState/FixState dapat sinyal error yang benar),
//           return. Entry point TIDAK dijalankan.
//        e. Kalau test PASS → lanjut ke langkah 6.
//   6. Build argv untuk entry point: {"python3", "<abs_path>"}.
//   7. runCommand(argv, timeout, "", sandbox_root, cancel_flag_)
//      — cwd = sandbox_root.
//   8. Simpan hasil ke ctx.lastExecution + update ctx.success/lastError.
//
// Stage 7g: cancel_flag_ diteruskan ke runCommand, jadi SIGKILL responsif
// saat user klik Cancel (bukan cuma menunggu timeout).
class ExecuteState : public IAgentState {
public:
    using ConfirmCallback = std::function<bool(const std::string& reason)>;

    ExecuteState(std::string                     sandbox_root,
                 frans::domain::IFileSystemPort& fs,
                 ConfirmCallback                 confirm             = nullptr,
                 int                             default_timeout_sec = 30,
                 std::atomic<bool>*              cancel_flag         = nullptr);

    std::string name() const override { return "Execute"; }
    void run(AgentContext& ctx) override;

private:
    std::string                     sandbox_root_;
    frans::domain::IFileSystemPort& fs_;
    ConfirmCallback                 confirm_;
    int                             default_timeout_sec_;
    std::atomic<bool>*              cancel_flag_ = nullptr;   // Stage 7g
};

} // namespace frans::application
