// ~/AgenAI/frans/src/application/agent/Agent.cpp
#include "application/agent/Agent.h"

#include "application/agent/AgentContext.h"
#include "application/agent/FixState.h"
#include "application/agent/GenerateState.h"
#include "application/agent/ObserveState.h"
#include "application/agent/PlanState.h"
#include "application/budget/DailyBudgetGuard.h"
#include "application/provider_router/ProviderRouter.h"
#include "code_intelligence/graph/DependencyGraph.h"
#include "domain/fs/IFileSystemPort.h"
#include "domain/llm/ILlmProvider.h"

#include <cstdio>
#include <utility>

namespace frans::application {

namespace {

const frans::code_intelligence::DependencyGraph& emptyGraph() {
    static const frans::code_intelligence::DependencyGraph g;
    return g;
}

void logStep(const char* state, int attempt, const AgentContext& ctx) {
    std::fprintf(stderr,
        "[frans][agent] attempt=%d step=%-8s success=%d%s%s\n",
        attempt, state, ctx.success ? 1 : 0,
        ctx.lastError.empty() ? "" : "  err=",
        ctx.lastError.empty() ? "" : ctx.lastError.c_str());
    std::fflush(stderr);
}

} // namespace

Agent::Agent(Deps deps) : deps_(std::move(deps)) {}

bool Agent::run(AgentContext& ctx) {
    // ---- Precondition (Stage 9c: provider ATAU router) ----
    if ((!deps_.router && !deps_.provider) || !deps_.fs) {
        ctx.success   = false;
        ctx.lastError = "Agent: Deps::fs belum di-set dan/atau tidak ada "
                        "provider (set deps.provider atau deps.router).";
        std::fprintf(stderr,
            "[frans][agent] ABORT: provider=%p router=%p fs=%p\n",
            static_cast<const void*>(deps_.provider),
            static_cast<const void*>(deps_.router),
            static_cast<const void*>(deps_.fs));
        std::fflush(stderr);
        return false;
    }

    auto is_cancelled = [this]() -> bool {
        return deps_.cancel_flag && deps_.cancel_flag->load();
    };

    auto announce = [this](const char* state, int attempt) {
        if (deps_.on_state) deps_.on_state(state, attempt);
    };

    // =====================================================================
    // 1. Plan
    // =====================================================================
    if (is_cancelled()) {
        ctx.success = false;
        ctx.lastError = "Cancelled before Plan.";
        return false;
    }
    {
        announce("Plan", 0);
        PlanState plan;
        plan.run(ctx);
        logStep("Plan", 0, ctx);
        if (!ctx.success) return false;
    }

    const auto& graph = deps_.graph ? *deps_.graph : emptyGraph();

    // Stage 9c — escalation state fresh per run.
    // Tidak disimpan di AgentContext supaya tidak bocor lintas-run
    // (AgentContext dipakai ulang oleh TaskRunner).
    EscalationContext esc_ctx;

    // =====================================================================
    // 2. Loop
    // =====================================================================
    for (ctx.attempt = 0; ctx.attempt < ctx.maxAttempts; ++ctx.attempt) {
        if (is_cancelled()) {
            ctx.success = false;
            ctx.lastError = "Cancelled by user.";
            std::fprintf(stderr,
                "[frans][agent] CANCELLED at attempt %d (before state).\n",
                ctx.attempt);
            std::fflush(stderr);
            return false;
        }

        const int attempt = ctx.attempt;

        // ---- Stage 9c: provider selection ----
        frans::domain::ILlmProvider* active_provider = deps_.provider;
        bool                         this_attempt_paid = false;

        if (deps_.router) {
            const double est_cost =
                deps_.router->estimatePaidCost(ctx.taskDescription);
            const RouteResult route = deps_.router->selectProvider(
                ctx.plannedComplexity, esc_ctx, est_cost);
            active_provider   = &route.provider;
            this_attempt_paid = route.is_paid;

            if (route.budget_blocked) {
                std::fprintf(stderr,
                    "[frans][agent] attempt=%d: budget API harian habis, "
                    "fallback ke provider lokal.\n", attempt);
                std::fflush(stderr);
            }
            std::fprintf(stderr,
                "[frans][agent] attempt=%d route: provider=%s paid=%d "
                "streak=%d escalated=%d\n",
                attempt, active_provider->name().c_str(),
                this_attempt_paid ? 1 : 0,
                esc_ctx.local_fail_streak, esc_ctx.escalated ? 1 : 0);
            std::fflush(stderr);
        }

        // ---- Generate / Fix ----
        if (attempt == 0) {
            announce("Generate", attempt);
            GenerateState gen(graph, *active_provider, *deps_.fs);
            gen.run(ctx);
            logStep("Generate", attempt, ctx);
        } else {
            announce("Fix", attempt);
            FixState fix(*active_provider, *deps_.fs);
            fix.run(ctx);
            logStep("Fix", attempt, ctx);
        }

        if (!ctx.success) return false;

        if (is_cancelled()) {
            ctx.success = false;
            ctx.lastError = "Cancelled by user.";
            return false;
        }

        // ---- Execute ----
        // Stage 7g: pass cancel_flag supaya runCommand bisa SIGKILL child
        // saat user klik Cancel (bukan cuma menunggu timeout).
        {
            announce("Execute", attempt);
            ExecuteState exec(deps_.sandbox_root,
                              *deps_.fs,
                              deps_.confirm,
                              deps_.execute_timeout_sec,
                              deps_.cancel_flag);
            exec.run(ctx);
            logStep("Execute", attempt, ctx);
        }

        // ---- Observe ----
        // Stage 7g: SELALU jalan, termasuk saat cancelled_by_user — supaya
        // observation-nya terekam dengan jelas (bukan langsung abort tanpa
        // jejak). Cancel tetap dihormati di bawah, setelah record tercatat.
        {
            announce("Observe", attempt);
            ObserveState observe;
            observe.run(ctx);
            logStep("Observe", attempt, ctx);
        }

        // ---- Catat attempt ----
        {
            AttemptRecord rec;
            rec.attempt     = attempt;
            rec.exit_code   = ctx.lastExecution.exit_code;
            rec.timed_out   = ctx.lastExecution.timed_out;
            rec.observation = ctx.lastObservation;
            ctx.attemptHistory.push_back(std::move(rec));
        }

        // ---- Stage 9c: escalation state + budget update ----
        // Dilakukan SETELAH Observe (supaya ctx.success final attempt ini
        // sudah terisi) dan SEBELUM cek success di bawah.
        //
        // recordUsage: hanya kalau attempt ini benar-benar pakai paid.
        //   ctx.lastCallCostUsd diisi oleh GenerateState/FixState tepat
        //   setelah provider.generate() return — 0.0 untuk provider lokal.
        // local_fail_streak: increment hanya kalau attempt ini LOCAL dan
        //   gagal. Attempt paid yang gagal TIDAK menaikkan streak (kita
        //   sudah di paid, tidak perlu eskalasi lagi — sticky policy yang
        //   menangani).
        if (deps_.router) {
            if (this_attempt_paid && deps_.budget_guard) {
                deps_.budget_guard->recordUsage(ctx.lastCallCostUsd);
            }
            if (!ctx.success && !this_attempt_paid) {
                ++esc_ctx.local_fail_streak;
            }
        }

        // Cancel check — dipindah ke sini (setelah Observe + record) supaya
        // cancel-by-execute tetap menghasilkan attempt record yang informatif.
        if (is_cancelled()) {
            ctx.success = false;
            ctx.lastError = "Cancelled by user.";
            std::fprintf(stderr,
                "[frans][agent] CANCELLED after attempt %d record.\n",
                attempt);
            std::fflush(stderr);
            return false;
        }

        if (ctx.success) return true;
    }

    std::fprintf(stderr,
        "[frans][agent] exhausted %d attempts, giving up.\n",
        ctx.maxAttempts);
    std::fflush(stderr);
    return false;
}

} // namespace frans::application
