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

        // ---- Generate / Fix ----
        if (attempt == 0) {
            announce("Generate", attempt);
            GenerateState gen(graph, *deps_.provider, *deps_.fs);
            gen.run(ctx);
            logStep("Generate", attempt, ctx);
        } else {
            announce("Fix", attempt);
            FixState fix(*deps_.provider, *deps_.fs);
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
                              deps_.execute_timeout_sec);
            exec.run(ctx);
            logStep("Execute", attempt, ctx);
        }

        if (is_cancelled()) {
            ctx.success = false;
            ctx.lastError = "Cancelled by user.";
            return false;
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

        if (ctx.success) return true;
    }

    std::fprintf(stderr,
        "[frans][agent] exhausted %d attempts, giving up.\n",
        ctx.maxAttempts);
    std::fflush(stderr);
    return false;
}

} // namespace frans::application
