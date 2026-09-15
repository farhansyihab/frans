// ~/AgenAI/frans/tests/router_3tier_smoke_test.cpp
//
// Stage 12 — Smoke test untuk ProviderRouter 3-tier + EscalationPolicy.
//
// Menggunakan mock ILlmProvider — tidak ada network call.
#include "application/provider_router/ProviderRouter.h"
#include "application/provider_router/EscalationPolicy.h"
#include "application/budget/DailyBudgetGuard.h"
#include "domain/llm/ILlmProvider.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

using namespace frans::application;
using namespace frans::domain;

namespace {

class MockProvider : public ILlmProvider {
public:
    MockProvider(std::string n, bool available = true)
        : name_(std::move(n)), available_(available) {}

    std::string name() const override { return name_; }
    bool        isAvailable() const override { return available_; }

    GenResult generate(const std::vector<ChatMessage>&,
                       const GenOptions&) override
    {
        return {};
    }
    GenResult streamGenerate(const std::vector<ChatMessage>&,
                             const GenOptions&,
                             TokenCallback,
                             const std::atomic<bool>*) override
    {
        return {};
    }
    CostEstimate estimateCost(const std::vector<ChatMessage>&) const override
    {
        CostEstimate c;
        c.input_usd  = 0.0;
        c.output_usd = 0.0;
        return c;
    }

private:
    std::string name_;
    bool        available_;
};

int g_fail = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        ++g_fail; \
    } \
} while (0)

} // namespace

int main() {
    std::printf("[router-3tier] starting...\n");

    MockProvider local("local");
    MockProvider gemini("gemini");
    MockProvider deepseek("deepseek");

    // ---- 1. Trivial → Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Trivial, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
        CHECK(!r.is_paid);
        CHECK(!r.budget_blocked);
    }

    // ---- 2. Moderate (fresh) → Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
    }

    // ---- 3. Moderate, local_fail_streak=1 → masih Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.local_fail_streak = 1;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &local);
    }

    // ---- 4. Moderate, local_fail_streak=2 → PaidLight ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.local_fail_streak = 2;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
        CHECK(r.is_paid);
        CHECK(ctx.max_tier_reached == RoutingTier::PaidLight);
    }

    // ---- 5. Complex → PaidLight langsung ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
    }

    // ---- 6. RequiresDeepReasoning → PaidHeavy langsung ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- 7. Sticky: max_tier=PaidLight + light_fail_streak=0 → PaidLight ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.max_tier_reached       = RoutingTier::PaidLight;
        ctx.paid_light_fail_streak = 0;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &gemini);
    }

    // ---- 8. Light fail 2x → escalate ke PaidHeavy ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.max_tier_reached       = RoutingTier::PaidLight;
        ctx.paid_light_fail_streak = 2;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- 9. PaidHeavy tidak tersedia → fallback PaidLight ----
    {
        ProviderRouter router(local, &gemini, nullptr, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.0);
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
    }

    // ---- 10. PaidLight tidak tersedia (nullptr) + policy PaLight → fallback PaidHeavy ----
    {
        // Constructor 3-tier tapi paid_light_ = nullptr.
        ProviderRouter router(local, nullptr, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        // Policy: Complex → PaidLight (max_tier belum pernah ke Heavy).
        // Router: paid_light null → fallback heavy.
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- 11. Gemini not available → fallback DeepSeek ----
    {
        MockProvider gemini_down("gemini", /*available=*/false);
        ProviderRouter router(local, &gemini_down, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- 12. 2-tier constructor (backward compat) ----
    {
        ProviderRouter router(local, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        // Policy → PaidLight. Router: paid_light_ null → fallback heavy.
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- 13. Semua paid unavailable → Local (bukan budget_blocked) ----
    {
        MockProvider gemini_down("gemini", false);
        MockProvider deepseek_down("deepseek", false);
        ProviderRouter router(local, &gemini_down, &deepseek_down, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
        CHECK(!r.budget_blocked);
    }

    // ---- 14. Budget blocked → fallback PaidLight (gratis) ----
    {
        // Budget guard dengan budget 0 = selalu blok.
        DailyBudgetGuard guard(/*budget=*/0.0, /*state_file=*/"");
        ProviderRouter router(local, &gemini, &deepseek, &guard);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.001);
        // Policy → PaidHeavy. Router: budget blok → fallback gemini.
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
        CHECK(r.budget_blocked);
    }

    // ---- 15. Budget blocked + no PaidLight → Local (budget_blocked=true) ----
    {
        DailyBudgetGuard guard(0.0, "");
        ProviderRouter router(local, nullptr, &deepseek, &guard);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.001);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
        CHECK(r.budget_blocked);
    }

    // ---- 16. EscalationPolicy langsung ----
    {
        EscalationPolicy policy;
        EscalationContext ctx;
        CHECK(policy.decide(ctx, TaskComplexity::Trivial) ==
              RoutingTier::Local);
        CHECK(policy.decide(ctx, TaskComplexity::Moderate) ==
              RoutingTier::Local);
        CHECK(policy.decide(ctx, TaskComplexity::Complex) ==
              RoutingTier::PaidLight);
        CHECK(policy.decide(ctx, TaskComplexity::RequiresDeepReasoning) ==
              RoutingTier::PaidHeavy);
    }

    if (g_fail > 0) {
        std::fprintf(stderr, "[router-3tier] %d check(s) FAILED\n", g_fail);
        return 1;
    }
    std::printf("[router-3tier] all checks passed\n");
    return 0;
}