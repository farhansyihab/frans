// ~/AgenAI/frans/tests/task_runner_smoke_test.cpp
//
// Smoke test Stage 7e: TaskRunner + multi-file parser + cancellation.
// Tidak butuh llama-server (MockProvider).
//
// Skenario:
//   1. MultiFileParser unit tests: single, multi, mixed, broken.
//   2. Single-file happy path via TaskRunner.
//   3. Multi-file happy path via TaskRunner.
//   4. Buggy → fix via TaskRunner.
//   5. Cancellation.

#include "application/agent/AgentContext.h"
#include "application/agent/MultiFileParser.h"
#include "application/task_runner/TaskRunner.h"
#include "domain/llm/ILlmProvider.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
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

class MockProvider : public ILlmProvider {
public:
    std::vector<std::string> responses;
    int                      call_count = 0;
    int                      sleep_ms_per_call = 0;
    std::vector<std::string> received_user_messages;

    std::string name() const override { return "Mock"; }
    bool isAvailable() const override { return true; }

    GenResult generate(const std::vector<ChatMessage>& messages,
                       const GenOptions&) override
    {
        if (sleep_ms_per_call > 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(sleep_ms_per_call));
        }

        GenResult r;
        r.success           = true;
        r.elapsed_sec       = 0.001;
        r.prompt_tokens     = 100;
        r.completion_tokens = 50;

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

std::string makeSandbox(const std::string& tag) {
    const std::string p =
        (fs::temp_directory_path()
         / ("frans_7e_test_" + tag + "_" + std::to_string(::getpid()))).string();
    std::error_code ec;
    fs::remove_all(p, ec);
    return p;
}

// Jalankan TaskRunner sampai selesai, auto-confirm YES.
TaskProgress runSync(TaskRunner& runner) {
    while (runner.isRunning()) {
        runner.poll();
        const std::string msg = runner.takePendingConfirmMessage();
        if (!msg.empty()) {
            runner.resolvePendingConfirm(true);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return runner.snapshot();
}

} // namespace

int main() {
    std::printf("=== frans_ai task runner smoke test (Stage 7e) ===\n");
    std::printf("Mock provider — llama-server TIDAK diperlukan.\n");

    // =====================================================================
    // Test 1 — MultiFileParser unit tests
    // =====================================================================
    banner("MultiFileParser — unit tests");
    {
        // (a) Single fenced block, no File: marker → fallback
        {
            const std::string out =
                "```python\nprint('hi')\n```\n";
            auto r = parseModelOutput(out, "main.py");
            check(r.files.size() == 1, "single: 1 file");
            check(!r.multi_file,       "single: multi_file = false");
            check(r.files[0].relative_path == "main.py", "single: path = main.py");
            check(r.files[0].content == "print('hi')", "single: content");
        }

        // (b) Multi-file, two markers
        {
            const std::string out =
                "File: hello.py\n"
                "```python\n"
                "print('a')\n"
                "```\n"
                "\n"
                "File: test.py\n"
                "```python\n"
                "print('b')\n"
                "```\n";
            auto r = parseModelOutput(out, "unused.py");
            check(r.files.size() == 2, "multi: 2 files");
            check(r.multi_file,        "multi: multi_file = true");
            check(r.files[0].relative_path == "hello.py", "multi[0] path");
            check(r.files[1].relative_path == "test.py",  "multi[1] path");
            check(r.files[0].content == "print('a')",     "multi[0] content");
            check(r.files[1].content == "print('b')",     "multi[1] content");
        }

        // (c) Case-insensitive marker + preamble noise
        {
            const std::string out =
                "Sure, here is the code:\n\n"
                "file: a.py\n"
                "```python\nprint('x')\n```\n\n"
                "FILE: b.py\n"
                "```python\nprint('y')\n```\n\n"
                "Done.\n";
            auto r = parseModelOutput(out, "z.py");
            check(r.files.size() == 2, "case-insensitive: 2 files");
            check(r.files[0].relative_path == "a.py", "case-insensitive: a.py");
            check(r.files[1].relative_path == "b.py", "case-insensitive: b.py");
        }

        // (d) Marker but no fence → error
        {
            const std::string out = "File: orphan.py\nno fence here\n";
            auto r = parseModelOutput(out, "main.py");
            check(r.multi_file, "broken: multi_file = true (marker seen)");
            check(r.files.empty(), "broken: no files extracted");
            check(!r.error.empty(), "broken: error terisi");
        }

        // (e) Empty response
        {
            auto r = parseModelOutput("", "main.py");
            check(r.files.empty(), "empty: no files");
            check(!r.error.empty(), "empty: error terisi");
        }

        // (f) Trailing newlines trimmed
        {
            const std::string out =
                "File: a.py\n```python\nprint('x')\n\n\n```\n";
            auto r = parseModelOutput(out, "fallback.py");
            check(r.files.size() == 1, "trim: 1 file");
            check(r.files[0].content == "print('x')", "trim: trailing \\n dihapus");
        }
    }

    // =====================================================================
    // Test 2 — Single-file happy path via TaskRunner
    // =====================================================================
    banner("TaskRunner — single-file happy path");
    {
        const std::string sandbox = makeSandbox("single");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            "```python\nprint('hello from single')\n```\n",
        };

        Agent::Deps adeps;
        adeps.provider            = &provider;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;

        AgentContext actx;
        actx.taskDescription    = "print hello";
        actx.targetRelativePath = "single.py";

        TaskRunner runner;
        check(runner.start(adeps, actx), "start task");

        TaskProgress p = runSync(runner);

        std::printf("  success=%d attempts=%zu files=%zu\n",
                    p.success ? 1 : 0, p.history.size(), p.files.size());
        std::printf("  stdout='%s'\n", p.stdout_text.c_str());

        check(p.done, "task done");
        check(p.success, "task success");
        check(p.history.size() == 1, "1 attempt");
        check(p.files.size() == 1, "1 file");
        check(p.files[0].relative_path == "single.py", "path benar");
        check(p.stdout_text == "hello from single\n", "stdout benar");
    }

    // =====================================================================
    // Test 3 — Multi-file happy path
    // =====================================================================
    banner("TaskRunner — multi-file happy path");
    {
        const std::string sandbox = makeSandbox("multi");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            "File: helper.py\n"
            "```python\n"
            "def greet():\n"
            "    return 'hi from helper'\n"
            "```\n"
            "\n"
            "File: main.py\n"
            "```python\n"
            "from helper import greet\n"
            "print(greet())\n"
            "```\n",
        };

        Agent::Deps adeps;
        adeps.provider            = &provider;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;

        AgentContext actx;
        actx.taskDescription    = "make a helper + main";
        actx.targetRelativePath = "main.py";

        TaskRunner runner;
        check(runner.start(adeps, actx), "start task");

        TaskProgress p = runSync(runner);

        std::printf("  success=%d attempts=%zu files=%zu\n",
                    p.success ? 1 : 0, p.history.size(), p.files.size());
        for (const auto& f : p.files) {
            std::printf("    - %s (%zu bytes)\n",
                        f.relative_path.c_str(), f.content.size());
        }
        std::printf("  stdout='%s'\n", p.stdout_text.c_str());

        check(p.done, "task done");
        check(p.success, "task success");
        check(p.history.size() == 1, "1 attempt");
        check(p.files.size() == 2, "2 files");
        check(p.stdout_text == "hi from helper\n", "stdout benar");

        // Verifikasi file kedua juga ada di sandbox.
        check(sb.exists("helper.py"), "helper.py ditulis");
        check(sb.exists("main.py"),   "main.py ditulis");
    }

    // =====================================================================
    // Test 4 — Buggy → fix via TaskRunner
    // =====================================================================
    banner("TaskRunner — buggy then fix");
    {
        const std::string sandbox = makeSandbox("fix");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.responses = {
            "```python\nraise ValueError('intentional')\n```\n",
            "```python\nprint('fixed')\n```\n",
        };

        Agent::Deps adeps;
        adeps.provider            = &provider;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;

        AgentContext actx;
        actx.taskDescription    = "print fixed";
        actx.targetRelativePath = "f.py";

        TaskRunner runner;
        check(runner.start(adeps, actx), "start task");

        TaskProgress p = runSync(runner);

        std::printf("  success=%d attempts=%zu\n",
                    p.success ? 1 : 0, p.history.size());
        for (const auto& a : p.history) {
            std::printf("    attempt %d: exit=%d\n", a.attempt, a.exit_code);
        }

        check(p.done, "done");
        check(p.success, "success");
        check(p.history.size() == 2, "2 attempts");
        check(p.history[0].exit_code != 0, "attempt 0 gagal");
        check(p.history[1].exit_code == 0, "attempt 1 sukses");
        check(p.stdout_text == "fixed\n", "stdout benar");
    }

    // =====================================================================
    // Test 5 — Cancellation
    //
    // MockProvider sleep 300ms per call. Task timeout 2s. Kami cancel
    // setelah 100ms. Worker harus abort sebelum attempt berikutnya.
    // =====================================================================
    banner("TaskRunner — cancellation");
    {
        const std::string sandbox = makeSandbox("cancel");
        SandboxedFileSystem sb(sandbox);

        MockProvider provider;
        provider.sleep_ms_per_call = 300;
        // Response yang akan menghasilkan loop panjang, tapi kita cancel
        // sebelum selesai.
        provider.responses = {
            "```python\nraise RuntimeError('will loop')\n```\n",
            "```python\nraise RuntimeError('still bad')\n```\n",
            "```python\nraise RuntimeError('still bad')\n```\n",
        };

        Agent::Deps adeps;
        adeps.provider            = &provider;
        adeps.fs                  = &sb;
        adeps.sandbox_root        = sb.root().string();
        adeps.execute_timeout_sec = 5;

        AgentContext actx;
        actx.taskDescription    = "loop forever";
        actx.targetRelativePath = "c.py";

        TaskRunner runner;
        check(runner.start(adeps, actx), "start task");

        // Tunggu 400ms, lalu request cancel.
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        runner.requestCancel();

        TaskProgress p = runSync(runner);

        std::printf("  done=%d cancelled=%d success=%d attempts=%zu\n",
                    p.done ? 1 : 0, p.cancelled ? 1 : 0,
                    p.success ? 1 : 0, p.history.size());

        check(p.done, "task done");
        check(!p.success, "not success");
        check(p.cancelled, "cancelled flag set");
        // Attempthistory harus < maxAttempts karena cancel dicek di batas state.
        check(p.history.size() < 3, "attemptHistory lebih pendek dari maxAttempts");
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
