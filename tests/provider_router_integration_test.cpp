// ~/AgenAI/frans/tests/provider_router_integration_test.cpp
//
// Smoke test Stage 9c: integrasi ProviderRouter + DailyBudgetGuard ke
// Agent loop. Menguji routing end-to-end lewat Agent::run() — bukan
// langsung lewat ProviderRouter (itu sudah diuji di
// provider_router_smoke_test.cpp).
//
// Semua provider adalah mock ILlmProvider — tidak butuh llama-server,
// tidak butuh API key, tidak butuh network.
//
// Skenario:
//   a. Task Complex  -> attempt 0 langsung pakai paid (skip local).
//   b. Task Moderate -> local gagal 2x, eskalasi ke paid.
//   c. Task Complex + budget $0 -> fallback ke local, paid tidak dipanggil.

#include "application/agent/Agent.h"
#include "application/agent/AgentContext.h"
#include "application/budget/DailyBudgetGuard.h"
#include "application/provider_router/ProviderRouter.h"
#include "domain/llm/ILlmProvider.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>

using namespace frans::application;
using frans::domain::ChatMessage;
using frans::domain::CostEstimate;
using frans::domain::GenOptions;
using frans::domain::GenResult;
using frans::domain::ILlmProvider;
using frans::domain::TokenCallback;
using frans::infrastructure::SandboxedFileSystem;

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

// ---------------------------------------------------------------------------
// MockProvider — antrian response canned. Setiap panggilan generate()
// pop satu response. Merekam call_count supaya test bisa memverifikasi
// berapa kali provider tertentu dipanggil.
// ---------------------------------------------------------------------------
class MockProvider : public ILlmProvider {
public:
    std::string              provider_name      = "Mock";
    std::vector<std::string> responses;
    int                      call_count        = 0;
    double                   cost_per_call_usd  = 0.0;
    bool                     available         = true;

    std::string name() const override { return provider_name; }
    bool isAvailable() const override { return available; }

    GenResult generate(const std::vector<ChatMessage>&,
                       const GenOptions&) override
    {
        GenResult r;
        r.success           = true;
        r.elapsed_sec       = 0.001;
        r.prompt_tokens     = 100;
        r.completion_tokens = 50;
        r.cost_usd          = cost_per_call_usd;

        if (call_count < static_cast<int>(responses.size())) {
            r.text = responses[call_count];
        } else {
            r.text = "```python\n# mock exhausted\n```\n";
        }
        ++call_count;
        return r;
    }

    GenResult streamGenerate(const std::vector<ChatMessage>&,
                             const GenOptions&,
                             TokenCallback,
                             const std::atomic<bool>*) override
    {
        GenResult r;
        r.error = "MockProvider does not implement streaming";
        return r;
    }

    CostEstimate estimateCost(const std::vector<ChatMessage>&) const override {
        CostEstimate c;
        // Untuk estimasi pra-generation, kita pakai cost_per_call_usd
        // sebagai approx input cost. ProviderRouter hanya butuh angka
        // > 0 untuk keputusan budget; nilai presisi tidak krusial di
        // level Agent.
        c.input_usd  = cost_per_call_usd;
        c.output_usd = 0.0;
        return c;
    }
};

std::string makeSandbox(const std::string& tag) {
    const std::string p = (fs::temp_directory_path()
        / ("frans_router_test_" + tag + "_" + std::to_string(::getpid()))).string();
    std::error_code ec;
    fs::remove_all(p, ec);
    return p;
}

std::string budgetPath(const std::string& tag) {
    return "/tmp/frans_router_budget_" + tag + "_"
         + std::to_string(::getpid()) + ".json";
}

std::string workingCode() {
    return "```python\nprint('ok from provider')\n```\n";
}

std::string failingCode(int n) {
    return "```python\nraise RuntimeError('fail "
         + std::to_string(n) + "')\n```\n";
}

} // namespace

int main() {
    std::printf("=== frans_ai provider router integration test (Stage 9c) ===\n");
    std::printf("Mock provider — llama-server TIDAK diperlukan.\n");

    // =====================================================================
    // Test a — Task Complex → attempt 0 langsung paid (skip local)
    // =====================================================================
    banner("a. Task Complex → langsung paid");
    {
        const std::string sandbox = makeSandbox("a");
        const std::string bpath   = budgetPath("a");
        std::error_code ec;
        fs::remove(bpath, ec);

        SandboxedFileSystem sb(sandbox);
        DailyBudgetGuard    guard(1.0, bpath);

        MockProvider local;
        local.provider_name = "LocalMock";
        // Seharusnya TIDAK dipanggil. Kalau ternyata dipanggil, generate
        // akan return kode yang raise → attempt 0 gagal → test akan
        // mendeteksi via local.call_count != 0.
        local.responses = { failingCode(0) };

        MockProvider paid;
        paid.provider_name     = "PaidMock";
        paid.cost_per_call_usd = 0.0005;
        paid.responses         = { workingCode() };

        ProviderRouter router(local, &paid, &guard);

        Agent::Deps adeps;
        adeps.provider            = &local;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;
        adeps.router              = &router;
        adeps.budget_guard        = &guard;

        AgentContext ctx;
        ctx.taskDescription    = "refactor the whole auth module";
        ctx.targetRelativePath = "main.py";

        Agent agent(adeps);
        const bool ok = agent.run(ctx);

        std::printf("  ok=%d  local.calls=%d  paid.calls=%d  "
                    "spent=$%.6f  cost_per_call=%.6f\n",
                    ok ? 1 : 0, local.call_count, paid.call_count,
                    guard.status().spent_usd, ctx.lastCallCostUsd);

        check(ok, "task sukses");
        check(ctx.success, "ctx.success == true");
        check(ctx.attempt == 0, "attempt == 0 (sekali jalan)");
        check(local.call_count == 0, "local TIDAK dipanggil");
        check(paid.call_count == 1, "paid dipanggil tepat 1x");
        check(guard.status().spent_usd > 0.0,
              "budget guard mencatat biaya > 0");
        check(ctx.attemptHistory.size() == 1, "1 attempt record");

        fs::remove(bpath, ec);
    }

    // =====================================================================
    // Test b — Task Moderate, local gagal 2x, eskalasi ke paid
    // =====================================================================
    banner("b. Task Moderate — local gagal 2x, eskalasi ke paid");
    {
        const std::string sandbox = makeSandbox("b");
        const std::string bpath   = budgetPath("b");
        std::error_code ec;
        fs::remove(bpath, ec);

        SandboxedFileSystem sb(sandbox);
        DailyBudgetGuard    guard(1.0, bpath);

        MockProvider local;
        local.provider_name = "LocalMock";
        local.responses = {
            failingCode(0),
            failingCode(1),
            failingCode(2),   // tidak akan tercapai
        };

        MockProvider paid;
        paid.provider_name     = "PaidMock";
        paid.cost_per_call_usd = 0.0005;
        paid.responses         = { workingCode() };

        ProviderRouter router(local, &paid, &guard);

        Agent::Deps adeps;
        adeps.provider            = &local;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;
        adeps.router              = &router;
        adeps.budget_guard        = &guard;

        AgentContext ctx;
        ctx.taskDescription    = "add a function that greets the user";
        ctx.targetRelativePath = "greet.py";

        Agent agent(adeps);
        const bool ok = agent.run(ctx);

        std::printf("  ok=%d  local.calls=%d  paid.calls=%d  attempt=%d  "
                    "history=%zu  spent=$%.6f\n",
                    ok ? 1 : 0, local.call_count, paid.call_count,
                    ctx.attempt, ctx.attemptHistory.size(),
                    guard.status().spent_usd);

        check(ok, "task sukses");
        check(ctx.success, "ctx.success == true");
        check(local.call_count == 2, "local dipanggil 2x (2x gagal)");
        check(paid.call_count == 1, "paid dipanggil 1x (eskalasi)");
        check(ctx.attempt == 2, "attempt == 2");
        check(ctx.attemptHistory.size() == 3, "3 attempt records");
        check(ctx.attemptHistory[0].exit_code != 0, "attempt 0 gagal");
        check(ctx.attemptHistory[1].exit_code != 0, "attempt 1 gagal");
        check(ctx.attemptHistory[2].exit_code == 0, "attempt 2 sukses");
        check(guard.status().spent_usd > 0.0,
              "budget guard mencatat biaya dari attempt paid");

        fs::remove(bpath, ec);
    }

    // =====================================================================
    // Test c — Task Complex + budget $0 → fallback ke local
    // =====================================================================
    banner("c. Task Complex + budget $0 → fallback ke local");
    {
        const std::string sandbox = makeSandbox("c");
        const std::string bpath   = budgetPath("c");
        std::error_code ec;
        fs::remove(bpath, ec);

        SandboxedFileSystem sb(sandbox);
        // Budget 0 → canCallPaidAPI() selalu false.
        DailyBudgetGuard    guard(0.0, bpath);

        MockProvider local;
        local.provider_name = "LocalMock";
        local.responses     = { workingCode() };

        MockProvider paid;
        paid.provider_name     = "PaidMock";
        paid.cost_per_call_usd = 0.0005;
        paid.responses         = { workingCode() };   // tidak boleh dipanggil

        ProviderRouter router(local, &paid, &guard);

        Agent::Deps adeps;
        adeps.provider            = &local;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;
        adeps.router              = &router;
        adeps.budget_guard        = &guard;

        AgentContext ctx;
        ctx.taskDescription    = "redesign the architecture across files";
        ctx.targetRelativePath = "main.py";

        Agent agent(adeps);
        const bool ok = agent.run(ctx);

        std::printf("  ok=%d  local.calls=%d  paid.calls=%d  attempt=%d  "
                    "spent=$%.6f\n",
                    ok ? 1 : 0, local.call_count, paid.call_count,
                    ctx.attempt, guard.status().spent_usd);

        check(ok, "task tetap selesai (fallback ke local)");
        check(ctx.success, "ctx.success == true");
        check(paid.call_count == 0, "paid TIDAK dipanggil (budget habis)");
        check(local.call_count == 1, "local dipanggil 1x (fallback)");
        check(guard.status().spent_usd == 0.0,
              "spent_usd tetap 0 (tidak ada paid call)");

        fs::remove(bpath, ec);
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
