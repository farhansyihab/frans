// ~/AgenAI/frans/src/application/agent/Agent.cpp
#include "application/agent/Agent.h"

#include "application/agent/AgentContext.h"
#include "application/agent/FixState.h"
#include "application/agent/GenerateState.h"
#include "application/agent/ObserveState.h"
#include "application/agent/PlanState.h"
#include "code_intelligence/graph/DependencyGraph.h"
#include "domain/fs/IFileSystemPort.h"
#include "domain/llm/ILlmProvider.h"

#include <cstdio>
#include <utility>

namespace frans::application {

namespace {

// Graph kosong statis — dipakai kalau Agent::Deps::graph == nullptr.
// Static function-local supaya tidak reconstruct tiap panggilan.
const frans::code_intelligence::DependencyGraph& emptyGraph() {
    static const frans::code_intelligence::DependencyGraph g;
    return g;
}

void logStep(const char* state,
             int attempt,
             const AgentContext& ctx)
{
    std::fprintf(stderr,
        "[frans][agent] attempt=%d step=%-8s success=%d%s%s\n",
        attempt,
        state,
        ctx.success ? 1 : 0,
        ctx.lastError.empty() ? "" : "  err=",
        ctx.lastError.empty() ? "" : ctx.lastError.c_str());
    std::fflush(stderr);
}

} // namespace

Agent::Agent(Deps deps) : deps_(std::move(deps)) {}

bool Agent::run(AgentContext& ctx) {
    // =====================================================================
    // 0. Sanity: deps wajib ada
    // =====================================================================
    if (!deps_.provider || !deps_.fs) {
        ctx.success   = false;
        ctx.lastError = "Agent: Deps::provider dan/atau Deps::fs belum "
                        "di-set (nullptr).";
        std::fprintf(stderr,
            "[frans][agent] ABORT: provider=%p fs=%p\n",
            static_cast<const void*>(deps_.provider),
            static_cast<const void*>(deps_.fs));
        std::fflush(stderr);
        return false;
    }

    // =====================================================================
    // 1. Plan
    // =====================================================================
    {
        PlanState plan;
        plan.run(ctx);
        logStep("Plan", 0, ctx);
        if (!ctx.success) return false;
    }

    const auto& graph = deps_.graph ? *deps_.graph : emptyGraph();

    // =====================================================================
    // 2. Loop Generate → Execute → Observe → Fix → ...
    // =====================================================================
    for (ctx.attempt = 0; ctx.attempt < ctx.maxAttempts; ++ctx.attempt) {
        const int attempt = ctx.attempt;

        // ---- Generate (attempt 0) atau Fix (attempt >= 1) ----
        if (attempt == 0) {
            GenerateState gen(graph, *deps_.provider, *deps_.fs);
            gen.run(ctx);
            logStep("Generate", attempt, ctx);
        } else {
            FixState fix(*deps_.provider, *deps_.fs);
            fix.run(ctx);
            logStep("Fix", attempt, ctx);
        }

        // Kalau generate/fix sendiri gagal (mis. LLM error, no fenced block),
        // tidak ada kode baru untuk dijalankan. Abort loop — retry tidak
        // akan menolong karena prompt berikutnya akan identik.
        if (!ctx.success) {
            return false;
        }

        // ---- Execute ----
        {
            ExecuteState exec(deps_.sandbox_root,
                              *deps_.fs,
                              deps_.confirm,
                              deps_.execute_timeout_sec);
            exec.run(ctx);
            // ExecuteState sendiri set ctx.success = false kalau gagal,
            // tapi kita tidak abort di sini — ObserveState perlu jalan
            // untuk menganalisis kenapa gagal.
            logStep("Execute", attempt, ctx);
        }

        // ---- Observe ----
        {
            ObserveState observe;
            observe.run(ctx);
            logStep("Observe", attempt, ctx);
        }

        // ---- Catat attempt SELALU (sukses maupun gagal) ----
        {
            AttemptRecord rec;
            rec.attempt     = attempt;
            rec.exit_code   = ctx.lastExecution.exit_code;
            rec.timed_out   = ctx.lastExecution.timed_out;
            rec.observation = ctx.lastObservation;
            ctx.attemptHistory.push_back(std::move(rec));
        }

        // ---- Sukses? Berhenti. ----
        if (ctx.success) {
            return true;
        }

        // Loop lanjut → FixState akan membaca observation yang baru saja
        // diisi ObserveState.
    }

    // =====================================================================
    // 3. Exhausted attempts
    // =====================================================================
    // ctx.success sudah false dari ObserveState terakhir.
    // ctx.lastError sudah diisi ringkasan kegagalan terakhir.
    std::fprintf(stderr,
        "[frans][agent] exhausted %d attempts, giving up.\n",
        ctx.maxAttempts);
    std::fflush(stderr);
    return false;
}

} // namespace frans::application
