// ~/AgenAI/frans/src/application/agent/Agent.h
#pragma once

#include "application/agent/ExecuteState.h"   // ExecuteState::ConfirmCallback

#include <string>

namespace frans::code_intelligence {
class DependencyGraph;
}

namespace frans::domain {
class ILlmProvider;
class IFileSystemPort;
}

namespace frans::application {

struct AgentContext;

// Blueprint §5.7 — Agent orchestrator (Stage 7d).
//
// Mengorkestrasi loop:
//
//   PlanState
//      │
//      ▼
//   ┌──────────────────────────────────────────────────┐
//   │  attempt 0: GenerateState                        │
//   │  attempt >0: FixState                            │
//   │      │                                           │
//   │      ▼                                           │
//   │  ExecuteState                                    │
//   │      │                                           │
//   │      ▼                                           │
//   │  ObserveState ─── success ──▶ return true        │
//   │      │                                           │
//   │      │ fail, attempt < maxAttempts               │
//   │      └──────────────────────┐                    │
//   └─────────────────────────────┼────────────────────┘
//                                 ▼
//                    return false (lastError berisi
//                                  ringkasan kegagalan)
//
// Design note: state classes tetap murni — mereka tidak tahu transisi.
// Agent yang memutuskan kapan pindah state. Ini menjaga IAgentState
// tetap minimal dan mudah di-test satu-satu.
//
// Stage 7d hanya memakai satu provider (local). ProviderRouter +
// EscalationPolicy masuk Stage 9 tanpa perubahan di Agent — cukup
// mengganti panggilan provider di dalam Generate/FixState.
class Agent {
public:
    // Struct "bundel dependensi". Default-constructible — isi field
    // satu per satu setelah deklarasi (pola sama seperti ChatDeps di
    // UI layer). Semua pointer boleh null KECUALI provider & fs;
    // Agent::run() akan menolak jalan kalau salah satu belum di-set.
    struct Deps {
        // ---- providers & io (WAJIB) ----
        frans::domain::ILlmProvider*    provider = nullptr;
        frans::domain::IFileSystemPort* fs       = nullptr;

        // ---- code intelligence (opsional) ----
        // Kalau null, GenerateState memakai graph kosong → context
        // selection tidak menghasilkan snippet. Aman untuk task yang
        // tidak butuh konteks project.
        const frans::code_intelligence::DependencyGraph* graph = nullptr;

        // ---- execution ----
        // Absolute canonical path (biasanya SandboxedFileSystem::root()).
        // Dipakai sebagai cwd child process.
        std::string sandbox_root;

        // Callback konfirmasi untuk ExecuteState (dipanggil dari thread
        // yang menjalankan Agent::run). Null → NeedsConfirm otomatis
        // ditolak.
        ExecuteState::ConfirmCallback confirm;

        // Timeout per attempt (detik).
        int execute_timeout_sec = 30;
    };

    explicit Agent(Deps deps);

    // Jalankan loop penuh. Modifikasi ctx in-place.
    // Return true kalau attempt terakhir sukses.
    //
    // Setelah run():
    //   ctx.success         — true/false
    //   ctx.attempt         — nomor attempt terakhir (0-based)
    //   ctx.attemptHistory  — semua attempt, termasuk yang sukses
    //   ctx.lastObservation — analisis error terakhir (kalau gagal)
    //   ctx.lastError       — ringkasan error terakhir (kalau gagal)
    //
    // Precondition: deps.provider != nullptr && deps.fs != nullptr.
    // Kalau tidak, run() langsung return false dengan lastError jelas.
    bool run(AgentContext& ctx);

private:
    Deps deps_;
};

} // namespace frans::application
