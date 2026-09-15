// ~/AgenAI/frans/tests/provider_router_smoke_test.cpp
//
// Smoke test Stage 9b: ProviderRouter.
//
// Menguji routing antara local & paid berdasarkan:
//   - kompleksitas task (via EscalationPolicy)
//   - riwayat attempt (EscalationContext.local_fail_streak & escalated)
//   - ketersediaan paid provider (isAvailable)
//   - budget guard (DailyBudgetGuard — nullable)
//
// Tidak butuh llama-server, tidak butuh API key, tidak butuh network —
// semua provider adalah mock ILlmProvider.

#include "application/budget/DailyBudgetGuard.h"
#include "application/provider_router/ProviderRouter.h"
#include "domain/llm/ILlmProvider.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

using frans::application::DailyBudgetGuard;
using frans::application::EscalationContext;
using frans::application::ProviderRouter;
using frans::application::TaskComplexity;
using frans::domain::ChatMessage;
using frans::domain::CostEstimate;
using frans::domain::GenOptions;
using frans::domain::GenResult;
using frans::domain::ILlmProvider;
using frans::domain::TokenCallback;

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& label) {
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", label.c_str());
    if (!cond) ++g_failures;
}

void banner(const std::string& s) {
    std::printf("\n=== %s ===\n", s.c_str());
}

std::string uniqueStatePath(const std::string& tag) {
    return "/tmp/frans_router_test_" + tag + "_"
         + std::to_string(::getpid()) + ".json";
}

void cleanup(const std::string& path) {
    std::error_code ec;
    fs::remove(path, ec);
}

// ---------------------------------------------------------------------------
// MockProvider — implementasi minimal ILlmProvider untuk test.
// ---------------------------------------------------------------------------
class MockProvider : public ILlmProvider {
public:
    std::string  provider_name = "Mock";
    bool         available     = true;    // toggle untuk test 3
    CostEstimate est           = {};      // configurable untuk test 7

    // Counter — verifikasi method benar-benar dipanggil.
    mutable int  estimate_cost_calls = 0;
    mutable int  is_available_calls  = 0;

    std::string name() const override { return provider_name; }

    bool isAvailable() const override {
        ++is_available_calls;
        return available;
    }

    GenResult generate(const std::vector<ChatMessage>&,
                       const GenOptions&) override
    {
        GenResult r;
        r.success = true;
        r.text    = "mock response";
        return r;
    }

    GenResult streamGenerate(const std::vector<ChatMessage>&,
                             const GenOptions&,
                             TokenCallback,
                             const std::atomic<bool>*) override
    {
        GenResult r;
        r.success = true;
        r.text    = "mock response";
        return r;
    }

    CostEstimate estimateCost(const std::vector<ChatMessage>&) const override {
        ++estimate_cost_calls;
        return est;
    }
};

} // namespace

int main() {
    std::printf("=== frans_ai provider router smoke test (Stage 9b) ===\n");

    // =====================================================================
    // 1. Trivial task, no escalation, streak=0 -> local
    // =====================================================================
    banner("1. Trivial task -> local");
    {
        MockProvider local;
        local.provider_name = "Local7B";
        MockProvider paid;
        paid.provider_name = "DeepSeek-Mock";

        ProviderRouter router(local, &paid, /*budget_guard=*/nullptr);

        EscalationContext esc;
        const auto r = router.selectProvider(
            TaskComplexity::Trivial, esc, /*estimated_cost_usd=*/0.0001);

        check(&r.provider == static_cast<ILlmProvider*>(&local),
              "provider == local");
        check(!r.is_paid,           "is_paid == false");
        check(!r.budget_blocked,    "budget_blocked == false");
        check(!esc.escalated,       "esc.escalated tetap false");
    }

    // =====================================================================
    // 2. Complex task -> paid (skip local)
    // =====================================================================
    banner("2. Complex task -> paid (skip local)");
    {
        MockProvider local;
        local.provider_name = "Local7B";
        MockProvider paid;
        paid.provider_name = "DeepSeek-Mock";
        paid.available     = true;

        ProviderRouter router(local, &paid, /*budget_guard=*/nullptr);

        EscalationContext esc;
        const auto r = router.selectProvider(
            TaskComplexity::Complex, esc, /*estimated_cost_usd=*/0.0005);

        check(&r.provider == static_cast<ILlmProvider*>(&paid),
              "provider == paid");
        check(r.is_paid,         "is_paid == true");
        check(!r.budget_blocked, "budget_blocked == false");
        check(esc.escalated,     "esc.escalated jadi true (sticky)");
    }

    // =====================================================================
    // 3. Paid isAvailable=false -> fallback local (bukan budget issue)
    // =====================================================================
    banner("3. Paid tidak available -> fallback local (bukan budget)");
    {
        MockProvider local;
        MockProvider paid;
        paid.provider_name = "DeepSeek-Mock";
        paid.available     = false;   // <-- kunci skenario

        ProviderRouter router(local, &paid, /*budget_guard=*/nullptr);

        EscalationContext esc;
        const auto r = router.selectProvider(
            TaskComplexity::Complex, esc, 0.0005);

        check(&r.provider == static_cast<ILlmProvider*>(&local),
              "provider == local (fallback)");
        check(!r.is_paid,           "is_paid == false");
        check(!r.budget_blocked,
              "budget_blocked == false (bukan soal budget)");
        check(!esc.escalated,
              "esc.escalated tetap false (eskalasi tidak terjadi)");
    }

    // =====================================================================
    // 4. budget_guard = nullptr -> paid selalu diizinkan
    // =====================================================================
    banner("4. budget_guard=nullptr -> paid selalu diizinkan");
    {
        MockProvider local;
        MockProvider paid;
        paid.provider_name = "DeepSeek-Mock";

        ProviderRouter router(local, &paid, /*budget_guard=*/nullptr);

        EscalationContext esc;
        const auto r = router.selectProvider(
            TaskComplexity::RequiresDeepReasoning, esc, 999.0);

        check(&r.provider == static_cast<ILlmProvider*>(&paid),
              "provider == paid (tanpa budget guard)");
        check(r.is_paid,         "is_paid == true");
        check(!r.budget_blocked, "budget_blocked == false");
        check(esc.escalated,     "esc.escalated == true");
    }

    // =====================================================================
    // 5. Budget habis -> fallback local + budget_blocked
    //
    // Catatan semantics: DailyBudgetGuard::canCallPaidAPI(cost) mengembalikan
    // true kalau (spent + cost) <= budget. Jadi spend PERSIS sampai budget
    // (0.01/0.01) tetap dianggap "bisa call" untuk cost=0.0 — itu boundary
    // semantics yang dikontrak oleh daily_budget_guard_smoke_test test 1.
    //
    // Supaya pre-check "budget benar-benar habis" valid, kita overspend
    // sedikit (0.02 > 0.01) sehingga spent > budget. Ini merepresentasikan
    // kondisi produksi yang realistis: user sudah melewati batas harian.
    // =====================================================================
    banner("5. Budget habis -> fallback local + budget_blocked");
    {
        const std::string path = uniqueStatePath("exhausted");
        cleanup(path);

        MockProvider local;
        MockProvider paid;
        paid.provider_name = "DeepSeek-Mock";

        // Budget $0.01, dihabiskan + overspend sedikit supaya kondisi
        // "exhausted" unambiguous di bawah semantics (spent + cost <= budget).
        DailyBudgetGuard guard(0.01, path);
        guard.recordUsage(0.02);

        check(!guard.canCallPaidAPI(0.0),
              "pre: budget benar-benar habis");

        ProviderRouter router(local, &paid, &guard);

        EscalationContext esc;
        const auto r = router.selectProvider(
            TaskComplexity::Complex, esc, /*estimated_cost_usd=*/0.0001);

        check(&r.provider == static_cast<ILlmProvider*>(&local),
              "provider == local (fallback karena budget habis)");
        check(!r.is_paid,           "is_paid == false");
        check(r.budget_blocked,
              "budget_blocked == true (sinyal ke caller)");
        check(!esc.escalated,
              "esc.escalated tetap false (eskalasi tidak terjadi)");

        cleanup(path);
    }

    // =====================================================================
    // 6. paid_ == nullptr -> selalu local, estimatePaidCost=0.0
    // =====================================================================
    banner("6. paid_=nullptr -> selalu local, estimatePaidCost=0.0");
    {
        MockProvider local;
        ProviderRouter router(local, /*paid=*/nullptr,
                                     /*budget_guard=*/nullptr);

        EscalationContext esc;
        const auto r = router.selectProvider(
            TaskComplexity::Complex, esc, 0.0);

        check(&r.provider == static_cast<ILlmProvider*>(&local),
              "provider == local");
        check(!r.is_paid,        "is_paid == false");
        check(!r.budget_blocked, "budget_blocked == false");
        check(!esc.escalated,    "esc.escalated tetap false");

        check(router.estimatePaidCost("refactor auth module") == 0.0,
              "estimatePaidCost() == 0.0 tanpa paid provider");
        check(!router.hasPaidProvider(),
              "hasPaidProvider() == false");
    }

    // =====================================================================
    // 7. estimatePaidCost — delegasi ke paid_->estimateCost()
    // =====================================================================
    banner("7. estimatePaidCost() delegasi ke paid_->estimateCost()");
    {
        MockProvider local;
        MockProvider paid;
        paid.est.input_usd  = 0.0010;
        paid.est.output_usd = 0.0020;

        ProviderRouter router(local, &paid, /*budget_guard=*/nullptr);

        const double c = router.estimatePaidCost("some task");
        check(c > 0.0, "estimatePaidCost() > 0 (dari provider)");
        check(paid.estimate_cost_calls >= 1,
              "paid_->estimateCost() benar-benar dipanggil");
    }

    // =====================================================================
    // 8. Sticky: sekali eskalasi, attempt berikutnya tetap paid
    // =====================================================================
    banner("8. Sticky: sekali eskalasi, tetap paid");
    {
        MockProvider local;
        MockProvider paid;
        paid.provider_name = "DeepSeek-Mock";

        ProviderRouter router(local, &paid, /*budget_guard=*/nullptr);

        EscalationContext esc;

        // Attempt 1: Complex -> escalate.
        const auto r1 = router.selectProvider(
            TaskComplexity::Complex, esc, 0.001);
        check(r1.is_paid,    "attempt 1: is_paid == true");
        check(esc.escalated, "attempt 1: esc.escalated == true");

        // Attempt 2: task jadi Moderate (misal fix kecil setelah Generate
        // pertama Complex). Sticky harus tetap paid.
        const auto r2 = router.selectProvider(
            TaskComplexity::Moderate, esc, 0.001);
        check(r2.is_paid,
              "attempt 2: Moderate tapi sticky -> tetap paid");
        check(&r2.provider == static_cast<ILlmProvider*>(&paid),
              "attempt 2: provider == paid");
        check(esc.escalated, "attempt 2: esc.escalated tetap true");
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
