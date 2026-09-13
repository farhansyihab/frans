// ~/AgenAI/frans/tests/agent_loop_smoke_test.cpp
//
// Smoke test Stage 7d: full agent loop dengan MOCK LLM provider.
// Tidak butuh llama-server — deterministik & cepat.
//
// Skenario:
//   0. PlanState classification: refactor/typo/default.
//   1. Happy path: 1 generate → execute sukses → selesai.
//   2. Fail-then-fix: buggy → observation → fix → sukses.
//   3. All fail: 3x buggy → agent menyerah, attemptHistory lengkap.
//   4. Timeout-then-fix: infinite loop → timeout → fix.
//   5. SyntaxError observation: syntax error → fix.
//   6. Danger check threading: kode BLOCKED → observation → fix.

#include "application/agent/Agent.h"
#include "application/agent/AgentContext.h"
#include "application/agent/PlanState.h"
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
// pop satu response. Kalau habis, return placeholder.
//
// Merekam semua user message yang diterima supaya test bisa memverifikasi
// bahwa FixState benar-benar menyuapkan observation ke prompt.
// ---------------------------------------------------------------------------
class MockProvider : public ILlmProvider {
public:
    std::vector<std::string> responses;
    int                      call_count = 0;
    std::vector<std::string> received_user_messages;

    std::string name() const override { return "Mock"; }
    bool isAvailable() const override { return true; }

    GenResult generate(const std::vector<ChatMessage>& messages,
                       const GenOptions&) override
    {
        GenResult r;
        r.success           = true;
        r.elapsed_sec       = 0.001;
        r.prompt_tokens     = 100;
        r.completion_tokens = 50;

        // Rekam user message terakhir.
        for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
            if (it->role == "user") {
                received_user_messages.push_back(it->content);
                break;
            }
        }

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
        return {};
    }
};

// Bungkus kode jadi fenced markdown block.
std::string fenced(const std::string& code) {
    return "```python\n" + code + "\n```\n";
}

// Sandbox bersih per test.
std::string makeSandbox(const std::string& tag) {
    const std::string p =
        (fs::temp_directory_path()
         / ("frans_agent_test_" + tag + "_"
            + std::to_string(::getpid()))).string();
    std::error_code ec;
    fs::remove_all(p, ec);
    return p;
}

} // namespace

int main() {
    std::printf("=== frans_ai agent loop smoke test (Stage 7d) ===\n");
    std::printf("Mock provider — llama-server TIDAK diperlukan.\n");

    // =====================================================================
    // Test 0 — PlanState classification
    // =====================================================================
    banner("PlanState — classification");
    {
        PlanState plan;
        AgentContext ctx;

        ctx.taskDescription    = "refactor the whole auth module";
        ctx.targetRelativePath = "auth/session.py";
        plan.run(ctx);
        check(ctx.success, "PlanState sukses untuk input valid");
        check(ctx.plannedComplexity == TaskComplexity::Complex,
              "\"refactor ... module\" → Complex");

        AgentContext ctx2;
        ctx2.taskDescription    = "fix typo in greeting";
        ctx2.targetRelativePath = "greet.py";
        plan.run(ctx2);
        check(ctx2.plannedComplexity == TaskComplexity::Trivial,
              "\"fix typo\" → Trivial");

        AgentContext ctx3;
        ctx3.taskDescription    = "add a helper function";
        ctx3.targetRelativePath = "helpers.py";
        plan.run(ctx3);
        check(ctx3.plannedComplexity == TaskComplexity::Moderate,
              "\"add a helper function\" → Moderate");

        // Invalid input
        AgentContext ctx4;
        ctx4.taskDescription    = "";
        ctx4.targetRelativePath = "x.py";
        plan.run(ctx4);
        check(!ctx4.success, "kosong taskDescription → fail");
    }

    // =====================================================================
    // Test 1 — Happy path: 1 attempt.
    // =====================================================================
    banner("Agent — happy path (1 attempt)");
    {
        const std::string sandbox = makeSandbox("happy");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = { fenced("print(2 + 2)") };

        Agent::Deps deps;
        deps.provider            = &provider;
        deps.fs                  = &sb;
        deps.graph               = nullptr;
        deps.sandbox_root        = sb.root().string();
        deps.execute_timeout_sec = 5;

        Agent agent(deps);

        AgentContext ctx;
        ctx.taskDescription    = "print 2+2 in Python";
        ctx.targetRelativePath = "calc.py";

        const bool ok = agent.run(ctx);

        std::printf("  provider calls = %d\n", provider.call_count);
        std::printf("  attempt = %d / max = %d\n",
                    ctx.attempt, ctx.maxAttempts);
        std::printf("  attemptHistory.size() = %zu\n",
                    ctx.attemptHistory.size());
        std::printf("  observation = %s\n", ctx.lastObservation.c_str());

        check(ok, "agent.run return true");
        check(ctx.success, "ctx.success == true");
        check(provider.call_count == 1, "1 LLM call");
        check(ctx.attempt == 0, "attempt == 0");
        check(ctx.attemptHistory.size() == 1, "1 record di history");
        check(ctx.lastExecution.exit_code == 0, "exit_code == 0");
        check(ctx.lastExecution.stdout_text == "4\n", "stdout == \"4\\n\"");
    }

    // =====================================================================
    // Test 2 — Fail then fix: 2 attempts.
    // =====================================================================
    banner("Agent — fail then fix (2 attempts)");
    {
        const std::string sandbox = makeSandbox("fix");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            fenced("raise ValueError('oops')"),
            fenced("print(2 + 2)"),
        };

        Agent::Deps deps;
        deps.provider            = &provider;
        deps.fs                  = &sb;
        deps.graph               = nullptr;
        deps.sandbox_root        = sb.root().string();
        deps.execute_timeout_sec = 5;

        Agent agent(deps);

        AgentContext ctx;
        ctx.taskDescription    = "print 2+2";
        ctx.targetRelativePath = "calc.py";

        const bool ok = agent.run(ctx);

        std::printf("  provider calls = %d\n", provider.call_count);
        std::printf("  attempt = %d / max = %d\n",
                    ctx.attempt, ctx.maxAttempts);
        std::printf("  attemptHistory.size() = %zu\n",
                    ctx.attemptHistory.size());
        for (const auto& a : ctx.attemptHistory) {
            std::printf("    attempt %d: exit=%d timed_out=%d\n",
                        a.attempt, a.exit_code, a.timed_out ? 1 : 0);
        }
        std::printf("  observation attempt 0:\n%s\n",
                    ctx.attemptHistory[0].observation.c_str());

        check(ok, "agent.run return true");
        check(ctx.success, "ctx.success == true");
        check(provider.call_count == 2, "2 LLM calls");
        check(ctx.attempt == 1, "attempt == 1");
        check(ctx.attemptHistory.size() == 2, "2 records di history");
        check(ctx.attemptHistory[0].exit_code != 0, "attempt 0 gagal");
        check(ctx.attemptHistory[1].exit_code == 0, "attempt 1 sukses");

        // Kritis: FixState harus benar-benar menerima error di prompt.
        const std::string& fix_prompt = provider.received_user_messages.back();
        check(fix_prompt.find("ValueError") != std::string::npos,
              "fix prompt mengandung 'ValueError'");
        check(fix_prompt.find("oops") != std::string::npos,
              "fix prompt mengandung pesan error 'oops'");
        check(fix_prompt.find("raise ValueError") != std::string::npos,
              "fix prompt mengandung kode lama yang buggy");
    }

    // =====================================================================
    // Test 3 — All fail: maxAttempts tercapai.
    // =====================================================================
    banner("Agent — all fail (maxAttempts reached)");
    {
        const std::string sandbox = makeSandbox("allfail");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            fenced("raise RuntimeError('attempt 1')"),
            fenced("raise RuntimeError('attempt 2')"),
            fenced("raise RuntimeError('attempt 3')"),
        };

        Agent::Deps deps;
        deps.provider            = &provider;
        deps.fs                  = &sb;
        deps.graph               = nullptr;
        deps.sandbox_root        = sb.root().string();
        deps.execute_timeout_sec = 5;

        Agent agent(deps);

        AgentContext ctx;
        ctx.taskDescription    = "make it run";
        ctx.targetRelativePath = "fail.py";

        const bool ok = agent.run(ctx);

        std::printf("  provider calls = %d\n", provider.call_count);
        std::printf("  attempt = %d / max = %d\n",
                    ctx.attempt, ctx.maxAttempts);
        std::printf("  final lastError = %s\n", ctx.lastError.c_str());

        check(!ok, "agent.run return false");
        check(!ctx.success, "ctx.success == false");
        check(provider.call_count == 3, "3 LLM calls");
        check(ctx.attemptHistory.size() == 3, "3 records di history");
        for (const auto& a : ctx.attemptHistory) {
            check(a.exit_code != 0, "semua attempt gagal");
        }
    }

    // =====================================================================
    // Test 4 — Timeout then fix.
    // =====================================================================
    banner("Agent — timeout then fix");
    {
        const std::string sandbox = makeSandbox("timeout");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            fenced("while True:\n    pass"),
            fenced("print('escaped')"),
        };

        Agent::Deps deps;
        deps.provider            = &provider;
        deps.fs                  = &sb;
        deps.graph               = nullptr;
        deps.sandbox_root        = sb.root().string();
        deps.execute_timeout_sec = 2;   // pendek supaya test cepat

        Agent agent(deps);

        AgentContext ctx;
        ctx.taskDescription    = "print escaped";
        ctx.targetRelativePath = "spin.py";

        const bool ok = agent.run(ctx);

        std::printf("  provider calls = %d\n", provider.call_count);
        for (const auto& a : ctx.attemptHistory) {
            std::printf("    attempt %d: exit=%d timed_out=%d\n",
                        a.attempt, a.exit_code, a.timed_out ? 1 : 0);
        }

        check(ok, "agent.run return true");
        check(provider.call_count == 2, "2 LLM calls");
        check(ctx.attemptHistory.size() == 2, "2 records");
        check(ctx.attemptHistory[0].timed_out, "attempt 0 timed out");
        check(ctx.attemptHistory[0].observation.find("TIMEOUT")
                  != std::string::npos,
              "observation attempt 0 menyebut TIMEOUT");

        // Fix prompt harus mengandung TIMEOUT.
        const std::string& fix_prompt = provider.received_user_messages.back();
        check(fix_prompt.find("TIMEOUT") != std::string::npos,
              "fix prompt mengandung 'TIMEOUT'");
    }

    // =====================================================================
    // Test 5 — SyntaxError observation.
    // =====================================================================
    banner("Agent — SyntaxError observation");
    {
        const std::string sandbox = makeSandbox("syntax");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            fenced("print(2 + 2"),   // kurung buka tidak ditutup
            fenced("print(2 + 2)"),
        };

        Agent::Deps deps;
        deps.provider            = &provider;
        deps.fs                  = &sb;
        deps.graph               = nullptr;
        deps.sandbox_root        = sb.root().string();
        deps.execute_timeout_sec = 5;

        Agent agent(deps);

        AgentContext ctx;
        ctx.taskDescription    = "print 2+2";
        ctx.targetRelativePath = "syn.py";

        const bool ok = agent.run(ctx);

        std::printf("  provider calls = %d\n", provider.call_count);
        if (!ctx.attemptHistory.empty()) {
            std::printf("  attempt 0 observation:\n%s\n",
                        ctx.attemptHistory[0].observation.c_str());
        }

        check(ok, "agent.run return true");
        check(provider.call_count == 2, "2 LLM calls");
        check(!ctx.attemptHistory.empty(), "attemptHistory tidak kosong");
        check(ctx.attemptHistory[0].exit_code != 0, "attempt 0 gagal");
        check(ctx.attemptHistory[0].observation.find("SyntaxError")
                  != std::string::npos,
              "observation attempt 0 menyebut 'SyntaxError'");

        const std::string& fix_prompt = provider.received_user_messages.back();
        check(fix_prompt.find("SyntaxError") != std::string::npos,
              "fix prompt mengandung 'SyntaxError'");
    }

    // =====================================================================
    // Test 6 — Danger check threading through Agent.
    // =====================================================================
    banner("Agent — danger check blocks execution, FixState retries clean");
    {
        const std::string sandbox = makeSandbox("danger");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            fenced("with open('/etc/passwd', 'w') as f:\n    f.write('x')"),
            fenced("print('safe now')"),
        };

        Agent::Deps deps;
        deps.provider            = &provider;
        deps.fs                  = &sb;
        deps.graph               = nullptr;
        deps.sandbox_root        = sb.root().string();
        deps.execute_timeout_sec = 5;
        // deps.confirm = nullptr: BLOCKED verdict tidak butuh konfirmasi.

        Agent agent(deps);

        AgentContext ctx;
        ctx.taskDescription    = "write to a file";
        ctx.targetRelativePath = "danger.py";

        const bool ok = agent.run(ctx);

        std::printf("  provider calls = %d\n", provider.call_count);
        if (!ctx.attemptHistory.empty()) {
            std::printf("  attempt 0 observation (first 200):\n%s\n",
                        ctx.attemptHistory[0].observation.substr(0, 200).c_str());
        }

        check(ok, "agent.run return true");
        check(provider.call_count == 2, "2 LLM calls");
        check(ctx.attemptHistory.size() == 2, "2 records");
        check(ctx.attemptHistory[0].exit_code != 0, "attempt 0 gagal (tidak dieksekusi)");

        // Observation attempt 0 harus membawa alasan dari ExecuteState
        // (via prior_error → lastObservation).
        const std::string& obs0 = ctx.attemptHistory[0].observation;
        check(obs0.find("BLOCKED") != std::string::npos
              || obs0.find("/etc/") != std::string::npos
              || obs0.find("skipped") != std::string::npos,
              "observation attempt 0 menyebut alasan blokir");

        // Fix prompt harus melihat alasan tersebut.
        const std::string& fix_prompt = provider.received_user_messages.back();
        check(fix_prompt.find("/etc/") != std::string::npos
              || fix_prompt.find("BLOCKED") != std::string::npos
              || fix_prompt.find("skipped") != std::string::npos,
              "fix prompt mengandung alasan blokir");
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
