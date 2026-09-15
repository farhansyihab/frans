// ~/AgenAI/frans/src/application/agent/Agent.h
#pragma once

#include "application/agent/ExecuteState.h"   // ExecuteState::ConfirmCallback

#include <atomic>
#include <functional>
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

// Blueprint Stage 9c — forward declarations. Hanya dipakai sebagai pointer
// di Deps, jadi tidak perlu include penuh di sini (menghindari siklus
// include: ProviderRouter.h sendiri include AgentContext.h untuk
// TaskComplexity).
class ProviderRouter;
class DailyBudgetGuard;

// Blueprint §5.7 — Agent orchestrator (Stage 7d).
// Stage 7e: tambah cancel flag + state transition callback.
// Stage 9c: tambah router + budget_guard (hybrid routing).
class Agent {
public:
    // Progress hook: dipanggil sebelum setiap state dijalankan.
    // `state_name` = "Plan" / "Generate" / "Fix" / "Execute" / "Observe".
    // `attempt`    = nomor attempt (0-based).
    //
    // WAJIB tidak throw. Dipanggil dari thread yang menjalankan Agent.
    using StateCallback = std::function<void(const std::string& state_name,
                                              int attempt)>;

    struct Deps {
        // ---- providers & io ----
        //
        // Precondition: minimal salah satu dari `provider` atau `router`
        // HARUS non-null, dan `fs` non-null. Kalau `router` di-set, ia
        // memilih provider tiap attempt (dan `provider` jadi fallback
        // kalau-kalau caller tidak memasang router). Kalau `router` null,
        // perilaku identik dengan sebelum Stage 9c: langsung pakai
        // `provider`.
        frans::domain::ILlmProvider*    provider = nullptr;
        frans::domain::IFileSystemPort* fs       = nullptr;

        // ---- Stage 9c: hybrid routing ----
        // Kalau non-null, Agent akan memanggil router->selectProvider()
        // setiap attempt dan memakai provider yang dipilih. ProviderRouter
        // menyentuh EscalationContext internal (fresh per run).
        ProviderRouter*   router       = nullptr;

        // Kalau non-null DAN attempt yang berjalan memakai paid provider,
        // Agent akan memanggil budget_guard->recordUsage(cost) setelah
        // attempt tersebut selesai (sebelum cek success).
        DailyBudgetGuard* budget_guard = nullptr;

        // ---- code intelligence (opsional) ----
        const frans::code_intelligence::DependencyGraph* graph = nullptr;

        // ---- execution ----
        std::string sandbox_root;
        ExecuteState::ConfirmCallback confirm;
        int execute_timeout_sec = 30;

        // ---- orchestration hooks (Stage 7e) ----
        // Kalau non-null dan nilainya true, Agent akan abort di batas state
        // berikutnya. Dicek SEBELUM setiap state, bukan mid-state.
        std::atomic<bool>* cancel_flag = nullptr;

        // Callback dipanggil sebelum setiap state.
        StateCallback on_state;
    };

    explicit Agent(Deps deps);

    // Jalankan loop penuh. Modifikasi ctx in-place.
    // Return true kalau attempt terakhir sukses.
    //
    // Precondition: deps.fs != nullptr DAN (deps.provider != nullptr ||
    //               deps.router != nullptr).
    bool run(AgentContext& ctx);

private:
    Deps deps_;
};

} // namespace frans::application
