// ~/AgenAI/frans/tests/router_3tier_smoke_test.cpp
//
// Stage 12 — Smoke test untuk ProviderRouter 3-tier + EscalationPolicy.
// Stage 12d — tambah kasus rate-limit cooldown.
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

    // =====================================================================
    // Bagian A — perilaku dasar (tidak ada cooldown)
    // =====================================================================

    // ---- A1. Trivial → Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Trivial, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
        CHECK(!r.is_paid);
        CHECK(!r.budget_blocked);
    }

    // ---- A2. Moderate (fresh) → Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
    }

    // ---- A3. Moderate, local_fail_streak=1 → masih Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.local_fail_streak = 1;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &local);
    }

    // ---- A4. Moderate, local_fail_streak=2 → PaidLight ----
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

    // ---- A5. Complex → PaidLight langsung ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
    }

    // ---- A6. RequiresDeepReasoning → PaidHeavy langsung ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- A7. Sticky: max_tier=PaidLight, light_fail=0 → PaidLight ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.max_tier_reached       = RoutingTier::PaidLight;
        ctx.paid_light_fail_streak = 0;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &gemini);
    }

    // ---- A8. Light fail 2x → escalate ke PaidHeavy ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.max_tier_reached       = RoutingTier::PaidLight;
        ctx.paid_light_fail_streak = 2;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- A9. PaidHeavy tidak tersedia → fallback PaidLight ----
    {
        ProviderRouter router(local, &gemini, nullptr, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.0);
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
    }

    // ---- A10. PaidLight null + policy PaLight → fallback PaidHeavy ----
    {
        ProviderRouter router(local, nullptr, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- A11. Gemini not available → fallback DeepSeek ----
    {
        MockProvider gemini_down("gemini", /*available=*/false);
        ProviderRouter router(local, &gemini_down, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- A12. 2-tier constructor (backward compat) ----
    {
        ProviderRouter router(local, &deepseek, nullptr);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- A13. Semua paid unavailable → Local ----
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

    // ---- A14. Budget blocked → fallback PaidLight (gratis) ----
    {
        DailyBudgetGuard guard(/*budget=*/0.0, /*state_file=*/"");
        ProviderRouter router(local, &gemini, &deepseek, &guard);
        EscalationContext ctx;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.001);
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
        CHECK(r.budget_blocked);
    }

    // ---- A15. Budget blocked + no PaidLight → Local (budget_blocked) ----
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

    // =====================================================================
    // Bagian B — Stage 12d: rate-limit cooldown
    //
    // Aturan: kalau cooldown > 0 dan tier IDEAL = PaidLight, downgrade
    // ke Local. PaidHeavy TIDAK terpengaruh (task berat tetap bisa
    // eskalasi ke DeepSeek).
    // =====================================================================

    // ---- B1. Cooldown=2 + Complex fresh → downgrade ke Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 2;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
        CHECK(!r.is_paid);
        CHECK(!r.budget_blocked);
    }

    // ---- B2. Cooldown=2 + Moderate + local_fail=2 → downgrade Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.local_fail_streak              = 2;
        ctx.paid_light_rate_limit_cooldown = 2;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
    }

    // ---- B3. Cooldown=2 + sticky PaidLight, streak=0 → downgrade Local ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.max_tier_reached               = RoutingTier::PaidLight;
        ctx.paid_light_fail_streak         = 0;
        ctx.paid_light_rate_limit_cooldown = 2;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
    }

    // ---- B4. Cooldown=2 + RequiresDeepReasoning → TETAP PaidHeavy ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 2;
        auto r = router.selectProvider(TaskComplexity::RequiresDeepReasoning,
                                        ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- B5. Cooldown=2 + max_tier=PaidHeavy (sticky) → TETAP PaidHeavy ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.max_tier_reached               = RoutingTier::PaidHeavy;
        ctx.paid_light_rate_limit_cooldown = 2;
        auto r = router.selectProvider(TaskComplexity::Trivial, ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- B6. Cooldown=2 + sticky PaidLight + light_fail=2 → PaidHeavy ----
    // Policy normal: naik ke PaidHeavy (Gemini sudah gagal 2x biasa).
    // Cooldown TIDAK memblok kenaikan ini — kita sudah give up on Gemini.
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.max_tier_reached               = RoutingTier::PaidLight;
        ctx.paid_light_fail_streak         = 2;
        ctx.paid_light_rate_limit_cooldown = 2;
        auto r = router.selectProvider(TaskComplexity::Moderate, ctx, 0.0);
        CHECK(&r.provider == &deepseek);
        CHECK(r.tier == RoutingTier::PaidHeavy);
    }

    // ---- B7. Cooldown=1 (edge) → tetap downgrade ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 1;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
    }

    // ---- B8. Cooldown=0 → tidak downgrade (PaidLight normal) ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 0;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
    }

    // ---- B9. Cooldown negatif (defensif) → tidak downgrade ----
    {
        ProviderRouter router(local, &gemini, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = -1;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &gemini);
        CHECK(r.tier == RoutingTier::PaidLight);
    }

    // ---- B10. Cooldown=2 + PaidLight null → TETAP downgrade Local ----
    //
    // Cooldown bersifat UNCONDITIONAL di level policy: tidak peduli
    // apakah PaidLight sebenarnya tersedia atau tidak. Karena cooldown
    // hanya di-set oleh Agent setelah PaidLight dipanggil dan return
    // 429/503, kombinasi "cooldown>0 + paid_light_==nullptr" tidak
    // akan pernah muncul di produksi — test ini murni mengunci
    // perilaku policy (downgrade unconditional).
    //
    // Catatan: tanpa cooldown, request ini akan fallback ke PaidHeavy
    // (lihat A10).
    {
        ProviderRouter router(local, nullptr, &deepseek, nullptr);
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 2;
        auto r = router.selectProvider(TaskComplexity::Complex, ctx, 0.0);
        CHECK(&r.provider == &local);
        CHECK(r.tier == RoutingTier::Local);
    }

    // =====================================================================
    // Bagian C — EscalationPolicy langsung (tanpa router)
    // =====================================================================

    // ---- C1. Aturan dasar tanpa cooldown ----
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

    // ---- C2. Cooldown 0 → tidak downgrade ----
    {
        EscalationPolicy policy;
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 0;
        CHECK(policy.decide(ctx, TaskComplexity::Complex) ==
              RoutingTier::PaidLight);
    }

    // ---- C3. Cooldown 1 → downgrade ----
    {
        EscalationPolicy policy;
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 1;
        CHECK(policy.decide(ctx, TaskComplexity::Complex) ==
              RoutingTier::Local);
    }

    // ---- C4. Cooldown 5 → downgrade (tidak ada upper bound khusus) ----
    {
        EscalationPolicy policy;
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 5;
        CHECK(policy.decide(ctx, TaskComplexity::Complex) ==
              RoutingTier::Local);
    }

    // ---- C5. Cooldown aktif + RequiresDeepReasoning → tetap PaidHeavy ----
    {
        EscalationPolicy policy;
        EscalationContext ctx;
        ctx.paid_light_rate_limit_cooldown = 5;
        CHECK(policy.decide(ctx, TaskComplexity::RequiresDeepReasoning) ==
              RoutingTier::PaidHeavy);
    }

    // ---- C6. Cooldown aktif + max_tier=PaidHeavy → tetap PaidHeavy ----
    {
        EscalationPolicy policy;
        EscalationContext ctx;
        ctx.max_tier_reached               = RoutingTier::PaidHeavy;
        ctx.paid_light_rate_limit_cooldown = 5;
        CHECK(policy.decide(ctx, TaskComplexity::Moderate) ==
              RoutingTier::PaidHeavy);
    }

    if (g_fail > 0) {
        std::fprintf(stderr, "[router-3tier] %d check(s) FAILED\n", g_fail);
        return 1;
    }
    std::printf("[router-3tier] all checks passed "
                "(termasuk 16 kasus Stage 12d cooldown)\n");
    return 0;
}
