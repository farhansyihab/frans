// ~/AgenAI/frans/tests/execute_smoke_test.cpp
//
// Smoke test Stage 7c: ExecuteState + DangerCheckHandler.
//
// Skenario:
//   0. DangerCheckHandler unit checks (plain code, subprocess, /etc/passwd).
//   1. Basic: print(2+2)               → stdout "4\n", exit=0, success.
//   2. Error: raise ValueError         → exit!=0, stderr ada "ValueError".
//   3. Timeout: while True: pass       → timed_out=true, durasi < 5s.
//   4. Blocked: /etc/passwd            → tidak dijalankan, lastError sebut BLOCKED.
//   5. NeedsConfirm tanpa callback     → tolak.
//   6. NeedsConfirm callback approve   → dijalankan.
//   7. Ekstensi .txt                   → fail dengan pesan jelas.
//   8. File tidak ada                  → fail dengan pesan jelas.
//
// Membutuhkan python3 di PATH (diasumsikan ada di Lubuntu default).

#include "application/agent/AgentContext.h"
#include "application/agent/ExecuteState.h"
#include "application/safety/DangerCheckHandler.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>

using frans::application::AgentContext;
using frans::application::DangerCheckHandler;
using frans::application::DangerVerdict;
using frans::application::ExecuteState;
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

} // namespace

int main() {
    const std::string sandbox_root =
        (fs::temp_directory_path()
         / ("frans_execute_test_" + std::to_string(::getpid()))).string();

    std::error_code ec;
    fs::remove_all(sandbox_root, ec);

    std::printf("=== frans_ai execute smoke test (Stage 7c) ===\n");
    std::printf("sandbox root (sebelum canonical): %s\n", sandbox_root.c_str());

    SandboxedFileSystem sb(sandbox_root);
    const std::string canonical = sb.root().string();
    std::printf("sandbox root (canonical)        : %s\n", canonical.c_str());

    // =====================================================================
    // Test 0 — DangerCheckHandler unit checks
    // =====================================================================
    banner("DangerCheckHandler — unit checks");
    {
        DangerCheckHandler d;

        const auto r1 = d.checkCode("print('hi')\n", "python");
        check(r1.verdict == DangerVerdict::Safe, "plain print → Safe");

        const auto r2 = d.checkCode(
            "import os\nos.system('ls')\n", "python");
        check(r2.verdict == DangerVerdict::NeedsConfirm,
              "os.system → NeedsConfirm");

        const auto r3 = d.checkCode(
            "open('/etc/passwd','w').write('x')\n", "python");
        check(r3.verdict == DangerVerdict::Blocked,
              "open('/etc/passwd') → Blocked");

        const auto r4 = d.checkCommand({"rm", "-rf", "/tmp/foo"});
        check(r4.verdict == DangerVerdict::NeedsConfirm,
              "argv rm -rf → NeedsConfirm");

        const auto r5 = d.checkCommand({"cat", "/etc/shadow"});
        check(r5.verdict == DangerVerdict::Blocked,
              "argv cat /etc/shadow → Blocked");
    }

    // =====================================================================
    // Test 1 — basic execution: print(2+2)
    // =====================================================================
    banner("ExecuteState — basic: print(2+2)");
    {
        sb.writeFile("calc.py", "print(2 + 2)\n");

        AgentContext ctx;
        ctx.targetRelativePath = "calc.py";

        ExecuteState exec(canonical, sb, /*confirm=*/nullptr, /*timeout=*/10);
        exec.run(ctx);

        std::printf("  stdout   : %s", ctx.lastExecution.stdout_text.c_str());
        std::printf("  stderr   : %s", ctx.lastExecution.stderr_text.c_str());
        std::printf("  exit=%d  timed_out=%d  success=%d  dur=%.2fs\n",
                    ctx.lastExecution.exit_code,
                    ctx.lastExecution.timed_out ? 1 : 0,
                    ctx.success ? 1 : 0,
                    ctx.lastExecution.duration_sec);

        check(ctx.success, "ctx.success == true");
        check(ctx.lastExecution.exit_code == 0, "exit_code == 0");
        check(ctx.lastExecution.stdout_text == "4\n",
              "stdout == \"4\\n\"");
        check(!ctx.lastExecution.timed_out, "not timed out");
    }

    // =====================================================================
    // Test 2 — error path: raise ValueError
    // =====================================================================
    banner("ExecuteState — error path: raise ValueError");
    {
        sb.writeFile("boom.py",
            "def fail():\n"
            "    raise ValueError('boom triggered')\n"
            "fail()\n");

        AgentContext ctx;
        ctx.targetRelativePath = "boom.py";

        ExecuteState exec(canonical, sb, nullptr, 10);
        exec.run(ctx);

        std::printf("  exit=%d  success=%d\n",
                    ctx.lastExecution.exit_code, ctx.success ? 1 : 0);
        std::printf("  stderr (first 220): %s\n",
                    ctx.lastExecution.stderr_text.substr(0, 220).c_str());

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastExecution.exit_code != 0, "exit_code != 0");
        check(ctx.lastExecution.stderr_text.find("ValueError")
                  != std::string::npos,
              "stderr mengandung 'ValueError'");
        check(ctx.lastExecution.stderr_text.find("boom triggered")
                  != std::string::npos,
              "stderr mengandung 'boom triggered'");
        check(!ctx.lastError.empty(), "ctx.lastError tidak kosong");
    }

    // =====================================================================
    // Test 3 — timeout: while True
    // =====================================================================
    banner("ExecuteState — timeout: while True");
    {
        sb.writeFile("spin.py", "while True:\n    pass\n");

        AgentContext ctx;
        ctx.targetRelativePath = "spin.py";

        ExecuteState exec(canonical, sb, nullptr, /*timeout_sec=*/2);
        exec.run(ctx);

        std::printf("  exit=%d  timed_out=%d  dur=%.2fs\n",
                    ctx.lastExecution.exit_code,
                    ctx.lastExecution.timed_out ? 1 : 0,
                    ctx.lastExecution.duration_sec);

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastExecution.timed_out, "timed_out == true");
        check(ctx.lastExecution.exit_code != 0, "exit_code != 0");
        check(ctx.lastExecution.duration_sec < 5.0,
              "duration < 5s (timeout benar-benar bekerja)");
    }

    // =====================================================================
    // Test 4 — danger: /etc/passwd reference (Blocked)
    // =====================================================================
    banner("ExecuteState — Blocked: /etc/passwd");
    {
        sb.writeFile("evil.py",
            "with open('/etc/passwd', 'w') as f:\n"
            "    f.write('x')\n");

        AgentContext ctx;
        ctx.targetRelativePath = "evil.py";

        ExecuteState exec(canonical, sb, nullptr, 5);
        exec.run(ctx);

        std::printf("  lastError: %s\n", ctx.lastError.c_str());

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastError.find("BLOCKED") != std::string::npos,
              "lastError menyebut BLOCKED");
        check(ctx.lastExecution.stdout_text.empty(),
              "tidak ada stdout (script tidak dijalankan)");
    }

    // =====================================================================
    // Test 5 — NeedsConfirm tanpa callback → tolak
    // =====================================================================
    banner("ExecuteState — NeedsConfirm, tanpa callback");
    {
        sb.writeFile("subproc.py",
            "import subprocess\n"
            "subprocess.run(['/bin/true'])\n"
            "print('done')\n");

        AgentContext ctx;
        ctx.targetRelativePath = "subproc.py";

        ExecuteState exec(canonical, sb, /*confirm=*/nullptr, 5);
        exec.run(ctx);

        std::printf("  lastError: %s\n", ctx.lastError.c_str());

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastError.find("konfirmasi") != std::string::npos
              || ctx.lastError.find("confirm")    != std::string::npos,
              "lastError menyebut konfirmasi");
    }

    // =====================================================================
    // Test 6 — NeedsConfirm dengan callback approve
    // =====================================================================
    banner("ExecuteState — NeedsConfirm, callback approve");
    {
        sb.writeFile("subproc_ok.py",
            "import subprocess\n"
            "r = subprocess.run(['/bin/echo', 'hello-from-subprocess'],\n"
            "                   capture_output=True, text=True)\n"
            "print(r.stdout.strip())\n");

        AgentContext ctx;
        ctx.targetRelativePath = "subproc_ok.py";

        int confirm_calls = 0;
        ExecuteState exec(canonical, sb,
            [&confirm_calls](const std::string&) -> bool {
                ++confirm_calls;
                return true;
            }, 10);
        exec.run(ctx);

        std::printf("  confirm_calls=%d  exit=%d  stdout='%s'\n",
                    confirm_calls,
                    ctx.lastExecution.exit_code,
                    ctx.lastExecution.stdout_text.c_str());

        check(confirm_calls == 1, "confirm callback dipanggil sekali");
        check(ctx.success, "ctx.success == true");
        check(ctx.lastExecution.stdout_text.find("hello-from-subprocess")
                  != std::string::npos,
              "stdout mengandung output subprocess");
    }

    // =====================================================================
    // Test 7 — ekstensi tidak didukung
    // =====================================================================
    banner("ExecuteState — ekstensi tidak didukung");
    {
        sb.writeFile("notes.txt", "hello world\n");

        AgentContext ctx;
        ctx.targetRelativePath = "notes.txt";

        ExecuteState exec(canonical, sb, nullptr, 5);
        exec.run(ctx);

        std::printf("  lastError: %s\n", ctx.lastError.c_str());

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastError.find("tidak didukung") != std::string::npos,
              "lastError menyebut ekstensi tidak didukung");
    }

    // =====================================================================
    // Test 8 — file tidak ada
    // =====================================================================
    banner("ExecuteState — file tidak ada");
    {
        AgentContext ctx;
        ctx.targetRelativePath = "ghost.py";

        ExecuteState exec(canonical, sb, nullptr, 5);
        exec.run(ctx);

        std::printf("  lastError: %s\n", ctx.lastError.c_str());

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastError.find("tidak ada") != std::string::npos,
              "lastError menyebut file tidak ada");
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    std::printf("sandbox tersisa di: %s\n", canonical.c_str());
    return g_failures == 0 ? 0 : 1;
}
