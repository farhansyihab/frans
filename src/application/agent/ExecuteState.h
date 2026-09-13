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
// Blueprint Stage 7f.5 — Test-aware execution.
//
// Alur (Stage 7f.5):
//   1. Validasi targetRelativePath & file-nya ada di sandbox.
//   2. Baca isi file → tentukan bahasa dari ekstensi (.py, .sh).
//   3. DangerCheckHandler::checkCode() → verdict untuk ENTRY POINT.
//   4. [BARU] Deteksi test file di top-level sandbox:
//        - nama match "test_*.py" atau "*_test.py", DAN
//        - isi mengandung "import unittest" atau "import pytest".
//   5. [BARU] Kalau test file ada DAN != targetRelativePath:
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
//   7. runCommand(argv, timeout, "", sandbox_root) — cwd = sandbox_root.
//   8. Simpan hasil ke ctx.lastExecution + update ctx.success/lastError.
//
// Prinsip (blueprint §5.7):
//   - Test adalah verifikasi pertama & termurah.
//   - Test FAIL sebelum side-effect entry point (tidak ada efek samping
//     dari script demo yang belum tentu benar).
//   - Test PASS → entry point sebagai smoke test tambahan (verifikasi
//     end-to-end, karena test tidak selalu cover __main__ block).
//
// Yang TIDAK dilakukan di sini:
//   - Parsing semantik output (ObserveState).
//   - Retry / eskalasi provider (FixState).
//
// Threading: run() biasanya dipanggil dari worker thread. ConfirmCallback
// dipanggil dari thread yang sama.
class ExecuteState : public IAgentState {
public:
    using ConfirmCallback = std::function<bool(const std::string& reason)>;

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
