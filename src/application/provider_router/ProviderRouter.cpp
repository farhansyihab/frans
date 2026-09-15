// ~/AgenAI/frans/src/application/provider_router/ProviderRouter.cpp
#include "application/provider_router/ProviderRouter.h"

#include "application/budget/DailyBudgetGuard.h"
#include "domain/llm/ILlmProvider.h"        // ILlmProvider + GenOptions (ChatMessage)

#include <cstdio>
#include <utility>
#include <vector>

namespace frans::application {

ProviderRouter::ProviderRouter(frans::domain::ILlmProvider& local,
                               frans::domain::ILlmProvider*  paid,
                               DailyBudgetGuard*             budget_guard,
                               EscalationPolicy              policy)
    : local_(local),
      paid_(paid),
      budget_guard_(budget_guard),
      policy_(std::move(policy)) {}

// ---------------------------------------------------------------------------
// estimatePaidCost
// ---------------------------------------------------------------------------
double ProviderRouter::estimatePaidCost(
    const std::string& task_description) const
{
    if (paid_ == nullptr) return 0.0;

    // Bangun vector ChatMessage minimal — ILlmProvider::estimateCost()
    // menerima vector<ChatMessage>. Kita hanya butuh approx input token
    // count, jadi satu pesan user berisi task_description sudah cukup.
    std::vector<frans::domain::ChatMessage> probe;
    probe.push_back({"user", task_description});

    const auto c = paid_->estimateCost(probe);
    return c.total_usd();
}

// ---------------------------------------------------------------------------
// selectProvider
// ---------------------------------------------------------------------------
RouteResult ProviderRouter::selectProvider(
    TaskComplexity      complexity,
    EscalationContext&  esc_ctx,
    double              estimated_cost_usd)
{
    const bool want_paid = policy_.shouldEscalate(esc_ctx, complexity);

    // ---- Tidak mau paid (policy bilang local) ----
    if (!want_paid) {
        return RouteResult{local_, /*is_paid=*/false, /*budget_blocked=*/false};
    }

    // ---- Policy bilang "harus paid" — tapi paid_ mungkin nullptr ----
    if (paid_ == nullptr) {
        std::fprintf(stderr,
            "[frans][router] policy=paid tapi paid provider nullptr "
            "(belum dikonfigurasi) -> fallback local\n");
        std::fflush(stderr);
        // budget_blocked=false: bukan soal budget, provider memang tidak ada.
        return RouteResult{local_, false, false};
    }

    // ---- paid ada tapi tidak available (api_key kosong / server down) ----
    if (!paid_->isAvailable()) {
        std::fprintf(stderr,
            "[frans][router] policy=paid tapi '%s' tidak available "
            "(api_key kosong / endpoint down) -> fallback local\n",
            paid_->name().c_str());
        std::fflush(stderr);
        return RouteResult{local_, false, false};
    }

    // ---- Budget check ----
    const bool budget_ok =
        (budget_guard_ == nullptr)
            ? true
            : budget_guard_->canCallPaidAPI(estimated_cost_usd);

    if (!budget_ok) {
        const double remaining = budget_guard_
            ? budget_guard_->remainingBudgetToday()
            : 0.0;
        std::fprintf(stderr,
            "[frans][router] BUDGET HABIS: estimate=$%.6f > remaining=$%.6f "
            "-> fallback ke local (task tetap dijalankan, tapi tanpa paid)\n",
            estimated_cost_usd, remaining);
        std::fflush(stderr);
        // es_ctx.escalated TIDAK di-set true: eskalasi tidak terjadi.
        return RouteResult{local_, false, /*budget_blocked=*/true};
    }

    // ---- Benar-benar naik ke paid ----
    // Set sticky flag HANYA di sini — sekali berhasil naik, tetap naik.
    esc_ctx.escalated = true;

    std::fprintf(stderr,
        "[frans][router] escalate -> '%s' (complexity=%d, estimate=$%.6f)\n",
        paid_->name().c_str(),
        static_cast<int>(complexity),
        estimated_cost_usd);
    std::fflush(stderr);

    return RouteResult{*paid_, /*is_paid=*/true, /*budget_blocked=*/false};
}

} // namespace frans::application
