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

// Blueprint §5.7 — Agent orchestrator (Stage 7d).
// Stage 7e: tambah cancel flag + state transition callback.
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
        // ---- providers & io (WAJIB) ----
        frans::domain::ILlmProvider*    provider = nullptr;
        frans::domain::IFileSystemPort* fs       = nullptr;

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
    // Precondition: deps.provider != nullptr && deps.fs != nullptr.
    bool run(AgentContext& ctx);

private:
    Deps deps_;
};

} // namespace frans::application
