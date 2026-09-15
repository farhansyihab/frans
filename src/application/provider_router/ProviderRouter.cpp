// ~/AgenAI/frans/src/application/provider_router/ProviderRouter.cpp
#include "application/provider_router/ProviderRouter.h"

#include "application/budget/DailyBudgetGuard.h"
#include "domain/llm/ILlmProvider.h"

#include <cstdio>
#include <utility>
#include <vector>

namespace frans::application {

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
ProviderRouter::ProviderRouter(frans::domain::ILlmProvider& local,
                                frans::domain::ILlmProvider*  paid_light,
                                frans::domain::ILlmProvider*  paid_heavy,
                                DailyBudgetGuard*             budget_guard,
                                EscalationPolicy              policy)
    : local_(local),
      paid_light_(paid_light),
      paid_heavy_(paid_heavy),
      budget_guard_(budget_guard),
      policy_(std::move(policy)) {}

ProviderRouter::ProviderRouter(frans::domain::ILlmProvider& local,
                                frans::domain::ILlmProvider*  paid,
                                DailyBudgetGuard*             budget_guard,
                                EscalationPolicy              policy)
    : local_(local),
      paid_light_(nullptr),
      paid_heavy_(paid),
      budget_guard_(budget_guard),
      policy_(std::move(policy)) {}

// ---------------------------------------------------------------------------
// Accessor
// ---------------------------------------------------------------------------
bool ProviderRouter::hasPaidProvider() const {
    return (paid_light_ != nullptr) || (paid_heavy_ != nullptr);
}
bool ProviderRouter::hasPaidLightProvider() const {
    return paid_light_ != nullptr;
}
bool ProviderRouter::hasPaidHeavyProvider() const {
    return paid_heavy_ != nullptr;
}

// ---------------------------------------------------------------------------
// estimatePaidCost
// ---------------------------------------------------------------------------
double ProviderRouter::estimatePaidCost(
    const std::string& task_description) const
{
    // Estimasi hanya relevan untuk PaidHeavy (berbayar). PaidLight
    // (Gemini) gratis — tidak butuh estimasi.
    if (paid_heavy_ == nullptr) return 0.0;

    std::vector<frans::domain::ChatMessage> probe;
    probe.push_back({"user", task_description});

    return paid_heavy_->estimateCost(probe).total_usd();
}

// ---------------------------------------------------------------------------
// selectProvider
// ---------------------------------------------------------------------------
RouteResult ProviderRouter::selectProvider(
    TaskComplexity      complexity,
    EscalationContext&  esc_ctx,
    double              estimated_cost_usd)
{
    const RoutingTier want_tier = policy_.decide(esc_ctx, complexity);

    // =====================================================================
    // 1. Local — policy bilang tetap lokal
    // =====================================================================
    if (want_tier == RoutingTier::Local) {
        return RouteResult{local_, /*is_paid=*/false,
                            /*budget_blocked=*/false,
                            RoutingTier::Local};
    }

    // =====================================================================
    // 2. PaidLight (Gemini — gratis)
    // =====================================================================
    if (want_tier == RoutingTier::PaidLight) {
        // 2a. Gemini tersedia → pakai.
        if (paid_light_ != nullptr && paid_light_->isAvailable()) {
            esc_ctx.max_tier_reached = RoutingTier::PaidLight;
            std::fprintf(stderr,
                "[frans][router] route -> '%s' (tier=PaidLight, "
                "complexity=%d, gratis)\n",
                paid_light_->name().c_str(),
                static_cast<int>(complexity));
            std::fflush(stderr);
            return RouteResult{*paid_light_, /*is_paid=*/true,
                                /*budget_blocked=*/false,
                                RoutingTier::PaidLight};
        }

        // 2b. Gemini tidak dikonfigurasi (paid_light_ == nullptr) →
        //     fallback ke PaidHeavy kalau ada. Ini mendukung setup
        //     2-tier (local + DeepSeek saja). Kalau paid_light_ ada
        //     tapi !isAvailable() (mis. api_key belum di-set setelah
        //     provider dibuat), kita juga fallback ke heavy — user
        //     tetap mengharapkan escalation paid.
        if (paid_heavy_ != nullptr && paid_heavy_->isAvailable()) {
            const bool budget_ok = (budget_guard_ == nullptr)
                ? true
                : budget_guard_->canCallPaidAPI(estimated_cost_usd);
            if (budget_ok) {
                esc_ctx.max_tier_reached = RoutingTier::PaidHeavy;
                std::fprintf(stderr,
                    "[frans][router] route -> '%s' (tier=PaidHeavy, "
                    "fallback dari PaidLight, estimate=$%.6f)\n",
                    paid_heavy_->name().c_str(), estimated_cost_usd);
                std::fflush(stderr);
                return RouteResult{*paid_heavy_, true, false,
                                    RoutingTier::PaidHeavy};
            }
            // Budget habis → fallback local.
            const double remaining = budget_guard_
                ? budget_guard_->remainingBudgetToday()
                : 0.0;
            std::fprintf(stderr,
                "[frans][router] BUDGET HABIS (fallback dari PaidLight): "
                "estimate=$%.6f > remaining=$%.6f -> local\n",
                estimated_cost_usd, remaining);
            std::fflush(stderr);
            return RouteResult{local_, false, /*budget_blocked=*/true,
                                RoutingTier::Local};
        }

        // 2c. Tidak ada paid yang tersedia → local (bukan budget_blocked;
        //     ini soal ketersediaan provider, bukan budget).
        std::fprintf(stderr,
            "[frans][router] policy=PaidLight tapi tidak ada paid "
            "provider yang tersedia -> fallback local\n");
        std::fflush(stderr);
        return RouteResult{local_, false, false, RoutingTier::Local};
    }

    // =====================================================================
    // 3. PaidHeavy (DeepSeek — berbayar)
    // =====================================================================
    // want_tier == RoutingTier::PaidHeavy

    if (paid_heavy_ != nullptr && paid_heavy_->isAvailable()) {
        const bool budget_ok = (budget_guard_ == nullptr)
            ? true
            : budget_guard_->canCallPaidAPI(estimated_cost_usd);

        if (budget_ok) {
            esc_ctx.max_tier_reached = RoutingTier::PaidHeavy;
            std::fprintf(stderr,
                "[frans][router] route -> '%s' (tier=PaidHeavy, "
                "complexity=%d, estimate=$%.6f)\n",
                paid_heavy_->name().c_str(),
                static_cast<int>(complexity),
                estimated_cost_usd);
            std::fflush(stderr);
            return RouteResult{*paid_heavy_, /*is_paid=*/true,
                                /*budget_blocked=*/false,
                                RoutingTier::PaidHeavy};
        }

        // Budget habis → fallback ke PaidLight (gratis) kalau ada.
        if (paid_light_ != nullptr && paid_light_->isAvailable()) {
            esc_ctx.max_tier_reached = RoutingTier::PaidLight;
            const double remaining = budget_guard_
                ? budget_guard_->remainingBudgetToday()
                : 0.0;
            std::fprintf(stderr,
                "[frans][router] BUDGET HABIS pada PaidHeavy: "
                "estimate=$%.6f > remaining=$%.6f -> fallback PaidLight "
                "('%s', gratis)\n",
                estimated_cost_usd, remaining,
                paid_light_->name().c_str());
            std::fflush(stderr);
            return RouteResult{*paid_light_, true, /*budget_blocked=*/true,
                                RoutingTier::PaidLight};
        }

        // Tidak ada PaidLight → local, dengan flag budget_blocked.
        const double remaining = budget_guard_
            ? budget_guard_->remainingBudgetToday()
            : 0.0;
        std::fprintf(stderr,
            "[frans][router] BUDGET HABIS dan tidak ada PaidLight: "
            "estimate=$%.6f > remaining=$%.6f -> fallback local\n",
            estimated_cost_usd, remaining);
        std::fflush(stderr);
        return RouteResult{local_, false, /*budget_blocked=*/true,
                            RoutingTier::Local};
    }

    // paid_heavy tidak tersedia → coba paid_light sebagai fallback
    // (meskipun policy minta PaidHeavy — PaidLight lebih baik dari local).
    if (paid_light_ != nullptr && paid_light_->isAvailable()) {
        esc_ctx.max_tier_reached = RoutingTier::PaidLight;
        std::fprintf(stderr,
            "[frans][router] policy=PaidHeavy tapi paid_heavy tidak "
            "tersedia -> fallback PaidLight ('%s', gratis)\n",
            paid_light_->name().c_str());
        std::fflush(stderr);
        return RouteResult{*paid_light_, true, false,
                            RoutingTier::PaidLight};
    }

    // Semua paid tidak tersedia → local (bukan budget_blocked; ini soal
    // ketersediaan provider, bukan budget).
    std::fprintf(stderr,
        "[frans][router] policy=PaidHeavy tapi tidak ada paid provider "
        "yang tersedia -> fallback local\n");
    std::fflush(stderr);
    return RouteResult{local_, false, false, RoutingTier::Local};
}

} // namespace frans::application