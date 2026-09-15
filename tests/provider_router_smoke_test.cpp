// ~/AgenAI/frans/tests/provider_router_smoke_test.cpp
//
// Stage 12 — ProviderRouter smoke test.
//
// Fokus: verifikasi fallback chain 3-tier + budget check + sticky behavior
// menggunakan mock provider (tanpa network).
//
// Perubahan dari versi 2-tier:
//   - `esc.escalated` (bool)          -> `esc.max_tier_reached` (RoutingTier)
//   - Constructor 2-tier tetap didukung untuk backward-compat:
//     `ProviderRouter(local, paid, budget)` memperlakukan `paid` sebagai
//     paid_heavy_. Karena PaidLight == nullptr, semua permintaan PaidLight
//     akan fallback ke PaidHeavy.
#include "application/budget/DailyBudgetGuard.h"
#include "application/provider_router/EscalationPolicy.h"
#include "application/provider_router/ProviderRouter.h"
#include "domain/llm/ILlmProvider.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <vector>

using namespace frans::application;
using namespace frans::domain;

namespace {

int g_fails = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        ++g_fails;
    }
}

// -------------------------------------------------------------------------
// Mock provider sederhana. Tidak ada HTTP; hanya menjawab name() dan
// isAvailable(). estimateCost selalu 0 (kecuali di-override).
// -------------------------------------------------------------------------
class StubProvider : public ILlmProvider {
public:
    StubProvider(std::string n, bool avail = true)
        : name_(std::move(n)), available_(avail) {}

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

} // namespace

int main() {
    std::printf("[provider_router_smoke] starting...\n");

    StubProvider local("local");
    StubProvider paid("paid-heavy");

    // =====================================================================
    // 1. Trivial → policy bilang Local → router pakai local.
    // =====================================================================
    {
        ProviderRouter router(local, &paid, nullptr);
        EscalationContext esc;
        auto r = router.selectProvider(TaskComplexity::Trivial, esc, 0.0);
        check(&r.provider == &local, "trivial: provider == local");
        check(!r.is_paid,            "trivial: !is_paid");
        check(r.tier == RoutingTier::Local, "trivial: tier == Local");
        check(!r.budget_blocked,    "trivial: !budget_blocked");
        check(esc.max_tier_reached == RoutingTier::Local,
              "trivial: esc.max_tier tetap Local");
    }

    // =====================================================================
    // 2. Complex fresh → policy bilang PaidLight → fallback PaidHeavy
    //    (karena paid_light_ == nullptr di constructor 2-tier).
    // =====================================================================
    {
        ProviderRouter router(local, &paid, nullptr);
        EscalationContext esc;
        auto r = router.selectProvider(TaskComplexity::Complex, esc, 0.0);
        check(&r.provider == &paid, "complex: fallback ke paid_heavy");
        check(r.is_paid,             "complex: is_paid");
        check(r.tier == RoutingTier::PaidHeavy,
              "complex: tier == PaidHeavy");
        check(esc.max_tier_reached == RoutingTier::PaidHeavy,
              "complex: esc.max_tier jadi PaidHeavy (sticky)");
    }

    // =====================================================================
    // 3. RequiresDeepReasoning → PaidHeavy.
    // =====================================================================
    {
        ProviderRouter router(local, &paid, nullptr);
        EscalationContext esc;
        auto r = router.selectProvider(
            TaskComplexity::RequiresDeepReasoning, esc, 0.0);
        check(&r.provider == &paid, "RDR: paid_heavy");
        check(r.tier == RoutingTier::PaidHeavy, "RDR: tier == PaidHeavy");
        check(esc.max_tier_reached == RoutingTier::PaidHeavy,
              "RDR: esc.max_tier == PaidHeavy");
    }

    // =====================================================================
    // 4. paid nullptr → fallback local (bukan budget_blocked).
    // =====================================================================
    {
        ProviderRouter router(local, nullptr, nullptr);
        EscalationContext esc;
        auto r = router.selectProvider(
            TaskComplexity::RequiresDeepReasoning, esc, 0.0);
        check(&r.provider == &local, "paid null: fallback local");
        check(!r.is_paid,             "paid null: !is_paid");
        check(!r.budget_blocked,
              "paid null: bukan budget_blocked (soal ketersediaan)");
        check(esc.max_tier_reached == RoutingTier::Local,
              "paid null: esc.max_tier tetap Local");
    }

    // =====================================================================
    // 5. paid tidak available → fallback local (bukan budget_blocked).
    // =====================================================================
    {
        StubProvider paid_down("paid-down", /*available=*/false);
        ProviderRouter router(local, &paid_down, nullptr);
        EscalationContext esc;
        auto r = router.selectProvider(TaskComplexity::Complex, esc, 0.0);
        check(&r.provider == &local, "paid down: fallback local");
        check(!r.is_paid,             "paid down: !is_paid");
        check(!r.budget_blocked,      "paid down: bukan budget_blocked");
        check(esc.max_tier_reached == RoutingTier::Local,
              "paid down: esc.max_tier tetap Local");
    }

    // =====================================================================
    // 6. Budget blocked (2-tier: paid_light null) → local, budget_blocked=true.
    // =====================================================================
    {
        DailyBudgetGuard guard(/*budget=*/0.0, /*state_file=*/"");
        ProviderRouter router(local, &paid, &guard);
        EscalationContext esc;
        auto r = router.selectProvider(
            TaskComplexity::RequiresDeepReasoning, esc, 0.01);
        check(&r.provider == &local, "budget blocked (2-tier): local");
        check(r.budget_blocked,      "budget blocked: flag true");
        check(esc.max_tier_reached == RoutingTier::Local,
              "budget blocked: esc.max_tier tetap Local");
    }

    // =====================================================================
    // 7. Budget OK → PaidHeavy (RDR).
    // =====================================================================
    {
        DailyBudgetGuard guard(/*budget=*/10.0, "");
        ProviderRouter router(local, &paid, &guard);
        EscalationContext esc;
        auto r = router.selectProvider(
            TaskComplexity::RequiresDeepReasoning, esc, 0.01);
        check(&r.provider == &paid,   "budget ok: paid_heavy");
        check(r.is_paid,               "budget ok: is_paid");
        check(!r.budget_blocked,       "budget ok: !budget_blocked");
        check(r.tier == RoutingTier::PaidHeavy,
              "budget ok: tier == PaidHeavy");
    }

    // =====================================================================
    // 8. local_fail_streak = 1 → tetap local.
    // =====================================================================
    {
        ProviderRouter router(local, &paid, nullptr);
        EscalationContext esc;
        esc.local_fail_streak = 1;
        auto r = router.selectProvider(TaskComplexity::Moderate, esc, 0.0);
        check(&r.provider == &local, "local_fail=1: tetap local");
        check(esc.max_tier_reached == RoutingTier::Local,
              "local_fail=1: esc tetap Local");
    }

    // =====================================================================
    // 9. local_fail_streak = 2 → naik (dalam 2-tier = PaidHeavy).
    // =====================================================================
    {
        ProviderRouter router(local, &paid, nullptr);
        EscalationContext esc;
        esc.local_fail_streak = 2;
        auto r = router.selectProvider(TaskComplexity::Moderate, esc, 0.0);
        check(&r.provider == &paid,   "local_fail=2: escalate");
        check(r.is_paid,               "local_fail=2: is_paid");
        check(r.tier == RoutingTier::PaidHeavy,
              "local_fail=2: tier PaidHeavy");
    }

    // =====================================================================
    // 10. Sticky: sekali eskalasi, request berikutnya tetap paid meski
    //     complexity turun.
    // =====================================================================
    {
        ProviderRouter router(local, &paid, nullptr);
        EscalationContext esc;

        // Attempt 1: Complex → paid_heavy.
        auto r1 = router.selectProvider(TaskComplexity::Complex, esc, 0.0);
        check(r1.is_paid, "sticky attempt 1: is_paid");
        check(esc.max_tier_reached == RoutingTier::PaidHeavy,
              "sticky attempt 1: max_tier = PaidHeavy");

        // Attempt 2: Trivial → sticky, tetap paid_heavy.
        auto r2 = router.selectProvider(TaskComplexity::Trivial, esc, 0.0);
        check(r2.is_paid, "sticky attempt 2: tetap paid");
        check(&r2.provider == &paid, "sticky attempt 2: provider == paid");
        check(esc.max_tier_reached == RoutingTier::PaidHeavy,
              "sticky attempt 2: max_tier tetap PaidHeavy");
    }

    // =====================================================================
    // 11. Accessor: hasPaidProvider / hasPaidLight / hasPaidHeavy.
    // =====================================================================
    {
        ProviderRouter r1(local, &paid, nullptr);          // 2-tier
        check(r1.hasPaidProvider(),       "2-tier: hasPaidProvider true");
        check(!r1.hasPaidLightProvider(), "2-tier: !hasPaidLight");
        check(r1.hasPaidHeavyProvider(),  "2-tier: hasPaidHeavy true");

        ProviderRouter r2(local, nullptr, nullptr);
        check(!r2.hasPaidProvider(),       "no-paid: hasPaidProvider false");
        check(!r2.hasPaidLightProvider(),  "no-paid: !hasPaidLight");
        check(!r2.hasPaidHeavyProvider(),  "no-paid: !hasPaidHeavy");

        // 3-tier constructor: light = &paid (misal), heavy = nullptr.
        ProviderRouter r3(local, &paid, nullptr, nullptr);
        check(r3.hasPaidProvider(),       "3-tier light: hasPaidProvider");
        check(r3.hasPaidLightProvider(),  "3-tier light: hasPaidLight");
        check(!r3.hasPaidHeavyProvider(), "3-tier light: !hasPaidHeavy");
    }

    // =====================================================================
    // 12. estimatePaidCost: null-safe.
    // =====================================================================
    {
        ProviderRouter router(local, nullptr, nullptr);
        check(router.estimatePaidCost("hello") == 0.0,
              "estimatePaidCost null -> 0.0");
    }

    if (g_fails > 0) {
        std::fprintf(stderr,
            "[provider_router_smoke] %d check(s) FAILED\n", g_fails);
        return 1;
    }
    std::printf("[provider_router_smoke] all checks passed\n");
    return 0;
}