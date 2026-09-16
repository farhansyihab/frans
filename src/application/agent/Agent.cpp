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
#include <string>
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

// Stage 12e follow-up — deteksi error "provider tidak reachable".
//
// Bukan task error (kode model salah), tapi infrastruktur error:
// provider lokal tidak jalan, network putus, endpoint down, dsb.
// Agent harus perlakukan ini sebagai sinyal "naik tier" (kalau bisa),
// bukan "task gagal → bail".
//
// String signature di bawah berasal dari cpp-httplib (`to_string(err)`)
// yang dibungkus oleh provider sebagai `GenResult.error`.
bool isProviderUnavailableError(const std::string& err) {
    if (err.empty()) return false;
    static const char* kSignatures[] = {
        "Could not establish connection",
        "Couldn't connect to server",
        "Connection refused",
        "Connection reset",
        "Timeout was reached",
    };
    for (const char* sig : kSignatures) {
        if (err.find(sig) != std::string::npos) return true;
    }
    return false;
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
        //
        // Stage 12e follow-up fix: pilih GenerateState kalau BELUM ada
        // observasi error yang bisa di-fix. Ini penting ketika attempt
        // sebelumnya di-skip karena provider unavailable — FixState
        // butuh `lastObservation` yang tidak kosong, sedangkan skip
        // tidak menghasilkan observasi.
        //
        // Aturan:
        //   attempt == 0                     -> Generate (kasus biasa)
        //   ctx.lastObservation kosong       -> Generate (skip-nya baru)
        //   selain itu                       -> Fix (perbaiki error)
        const bool use_generate =
            (attempt == 0) || ctx.lastObservation.empty();

        if (use_generate) {
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

        // ---- Stage 12e follow-up: provider unavailable → naik tier ----
        //
        // Kalau error-nya khas koneksi putus DAN kita masih bisa naik
        // tier, treat sebagai sinyal eskalasi — record attempt,
        // increment streak (+2 untuk capai threshold langsung), lalu
        // `continue` ke attempt berikutnya.
        if (!ctx.success) {
            const bool is_provider_error =
                deps_.router && isProviderUnavailableError(ctx.lastError);

            if (is_provider_error
                && this_attempt_tier != RoutingTier::PaidHeavy) {
                std::fprintf(stderr,
                    "[frans][agent] attempt=%d: provider '%s' (tier=%s) "
                    "unavailable — naik tier, skip Execute.\n",
                    attempt, active_provider->name().c_str(),
                    tierName(this_attempt_tier));
                std::fflush(stderr);

                // Record attempt supaya UI bisa menampilkan jejaknya.
                {
                    AttemptRecord rec;
                    rec.attempt     = attempt;
                    rec.exit_code   = -1;
                    rec.timed_out   = false;
                    rec.observation = "Provider '" + active_provider->name()
                                    + "' (" + tierName(this_attempt_tier)
                                    + ") tidak tersedia: " + ctx.lastError;
                    ctx.attemptHistory.push_back(std::move(rec));
                }

                // Streak +2 → capai threshold policy di attempt berikutnya.
                if (this_attempt_tier == RoutingTier::Local) {
                    esc_ctx.local_fail_streak += 2;
                } else if (this_attempt_tier == RoutingTier::PaidLight) {
                    esc_ctx.paid_light_fail_streak += 2;
                }

                // Jangan bail — loop ke attempt berikutnya.
                continue;
            }

            // Task error sejati (kode model salah) ATAU PaidHeavy down
            // ATAU mode legacy tanpa router → bail.
            return false;
        }

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
                esc_ctx.local_fail_streak              = 0;
                esc_ctx.paid_light_fail_streak         = 0;
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
            }

            // 3. Decrement cooldown.
            if (!just_set_cooldown
                && esc_ctx.paid_light_rate_limit_cooldown > 0) {
                --esc_ctx.paid_light_rate_limit_cooldown;
            }
        }

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