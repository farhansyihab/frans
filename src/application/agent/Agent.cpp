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

    // Stage 9c + 12 — escalation state fresh per run.
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
                "paid=%d local_streak=%d light_streak=%d "
                "cooldown=%d max_tier=%s\n",
                attempt, active_provider->name().c_str(),
                tierName(this_attempt_tier),
                this_attempt_paid ? 1 : 0,
                esc_ctx.local_fail_streak,
                esc_ctx.paid_light_fail_streak,
                esc_ctx.paid_light_rate_limit_cooldown,
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
        //
        // Urutan:
        //   1. Budget / audit per tier (PaidHeavy -> recordUsage,
        //      PaidLight -> recordFreeTierUsage).
        //   2. Streak / cooldown update.
        //   3. Decrement cooldown (kalau attempt ini bukan yang baru
        //      men-set cooldown).
        //
        // Aturan streak:
        //   Sukses                       -> reset semua streak + cooldown.
        //   Local gagal                  -> local_fail_streak++.
        //   PaidLight gagal (biasa)      -> paid_light_fail_streak++.
        //   PaidLight gagal (rate limit) -> cooldown=2, JANGAN increment
        //                                   paid_light_fail_streak.
        //   PaidHeavy gagal              -> no-op (sticky menangani).
        //
        // Alasan rate-limit TIDAK increment paid_light_fail_streak:
        // blueprint §10 — rate limit BUKAN alasan naik ke DeepSeek.
        // Rate limit = kuota sementara; kita tunggu sambil pakai Local.
        if (deps_.router) {
            bool just_set_cooldown = false;

            // 1. Budget / audit.
            if (deps_.budget_guard) {
                if (this_attempt_tier == RoutingTier::PaidHeavy) {
                    deps_.budget_guard->recordUsage(ctx.lastCallCostUsd);
                } else if (this_attempt_tier == RoutingTier::PaidLight) {
                    deps_.budget_guard->recordFreeTierUsage(
                        ctx.lastPromptTokens,
                        ctx.lastCompletionTokens);
                }
            }

            // 2. Streak / cooldown.
            if (ctx.success) {
                esc_ctx.local_fail_streak            = 0;
                esc_ctx.paid_light_fail_streak       = 0;
                esc_ctx.paid_light_rate_limit_cooldown = 0;
            } else {
                if (this_attempt_tier == RoutingTier::Local) {
                    ++esc_ctx.local_fail_streak;
                } else if (this_attempt_tier == RoutingTier::PaidLight) {
                    if (ctx.lastCallRateLimited) {
                        esc_ctx.paid_light_rate_limit_cooldown = 2;
                        just_set_cooldown = true;
                        std::fprintf(stderr,
                            "[frans][agent] attempt=%d: PaidLight "
                            "rate-limited — cooldown=2 attempt "
                            "(fallback Local, TIDAK naik ke PaidHeavy).\n",
                            attempt);
                        std::fflush(stderr);
                    } else {
                        ++esc_ctx.paid_light_fail_streak;
                    }
                }
                // PaidHeavy gagal: no-op.
            }

            // 3. Decrement cooldown. Skip kalau attempt ini BARU
            //    men-set cooldown (biar nilainya tidak langsung turun).
            if (!just_set_cooldown
                && esc_ctx.paid_light_rate_limit_cooldown > 0) {
                --esc_ctx.paid_light_rate_limit_cooldown;
            }
        }

        // Cancel check — dipindah ke sini (setelah Observe + record).
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