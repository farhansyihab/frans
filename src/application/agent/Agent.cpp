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

const char* tierName(RoutingTier t) {
    switch (t) {
        case RoutingTier::Local:     return "Local";
        case RoutingTier::PaidLight: return "PaidLight";
        case RoutingTier::PaidHeavy: return "PaidHeavy";
    }
    return "?";
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

    // Stage 9c + Stage 12 — escalation state fresh per run.
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

        // ---- Stage 12: provider selection (3-tier) ----
        frans::domain::ILlmProvider* active_provider = deps_.provider;
        bool                         this_attempt_paid = false;
        RoutingTier                  this_attempt_tier = RoutingTier::Local;

        if (deps_.router) {
            const double est_cost =
                deps_.router->estimatePaidCost(ctx.taskDescription);
            const RouteResult route = deps_.router->selectProvider(
                ctx.plannedComplexity, esc_ctx, est_cost);
            active_provider    = &route.provider;
            this_attempt_paid  = route.is_paid;
            this_attempt_tier  = route.tier;

            if (route.budget_blocked) {
                std::fprintf(stderr,
                    "[frans][agent] attempt=%d: budget API harian habis, "
                    "fallback ke tier bawah (%s).\n",
                    attempt, tierName(route.tier));
                std::fflush(stderr);
            }
            std::fprintf(stderr,
                "[frans][agent] attempt=%d route: provider=%s tier=%s "
                "paid=%d local_streak=%d light_streak=%d max_tier=%s\n",
                attempt, active_provider->name().c_str(),
                tierName(this_attempt_tier),
                this_attempt_paid ? 1 : 0,
                esc_ctx.local_fail_streak,
                esc_ctx.paid_light_fail_streak,
                tierName(esc_ctx.max_tier_reached));
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

        // ---- Stage 12: escalation state + budget update ----
        // Dilakukan SETELAH Observe (supaya ctx.success final attempt ini
        // sudah terisi) dan SEBELUM cek success di bawah.
        //
        // Aturan update:
        //   PaidHeavy selesai (sukses/gagal) -> recordUsage (biaya nyata).
        //   PaidLight selesai (sukses/gagal) -> recordFreeTierUsage (audit).
        //   PaidLight sukses                 -> reset kedua streak.
        //   PaidHeavy sukses                 -> reset kedua streak.
        //   Local gagal                      -> local_fail_streak++
        //   PaidLight gagal                  -> paid_light_fail_streak++
        //   PaidHeavy gagal                  -> tidak increment (sticky).
        //
        // ctx.lastCallCostUsd diisi oleh GenerateState/FixState tepat
        // setelah provider.generate() return — 0.0 untuk provider gratis
        // (Local, Gemini). Hanya dipakai untuk PaidHeavy.
        //
        // CATATAN Stage 12: audit token free-tier (Gemini) sementara
        // dilewati karena AgentContext belum menyimpan prompt/completion
        // tokens dari panggilan terakhir. TODO: tambah field
        // `lastPromptTokens` / `lastCompletionTokens` ke AgentContext,
        // isi di GenerateState/FixState setelah provider.generate(),
        // lalu aktifkan recordFreeTierUsage di bawah.
        if (deps_.router) {
            // 1. Budget per tier.
            if (deps_.budget_guard) {
                if (this_attempt_tier == RoutingTier::PaidHeavy) {
                    deps_.budget_guard->recordUsage(ctx.lastCallCostUsd);
                }
                // else if (this_attempt_tier == RoutingTier::PaidLight) {
                //     deps_.budget_guard->recordFreeTierUsage(
                //         ctx.lastPromptTokens,
                //         ctx.lastCompletionTokens);
                // }
            }

            // 2. Streak update.
            if (ctx.success) {
                // Attempt ini sukses — reset kedua streak supaya attempt
                // berikutnya (kalau ada, mis. multi-task) mulai dari Local
                // lagi kecuali max_tier_reached sudah sticky.
                esc_ctx.local_fail_streak      = 0;
                esc_ctx.paid_light_fail_streak = 0;
            } else {
                if (this_attempt_tier == RoutingTier::Local) {
                    ++esc_ctx.local_fail_streak;
                } else if (this_attempt_tier == RoutingTier::PaidLight) {
                    ++esc_ctx.paid_light_fail_streak;
                }
                // PaidHeavy gagal: tidak increment. Sticky di max_tier
                // sudah menjamin attempt berikutnya tetap di PaidHeavy.
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