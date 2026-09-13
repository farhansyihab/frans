// ~/AgenAI/frans/tests/execute_smoke_test.cpp
//
// Smoke test Stage 7c + Stage 7f.5.
//
// Stage 7c (existing):
//   0. DangerCheckHandler unit checks.
//   1. Basic execution: print(2+2).
//   2. Error: raise ValueError.
//   3. Timeout: while True.
//   4. Blocked: /etc/passwd.
//   5. NeedsConfirm tanpa callback.
//   6. NeedsConfirm callback approve.
//   7. Ekstensi tidak didukung.
//   8. File tidak ada.
//
// Stage 7f.5 (new):
//   9.  Test-aware: test pass + entry pass.
//   10. Test-aware: test fail → entry NOT run.
//   11. Test-aware: no test file → entry run saja.
//   12. Test-aware: nama test_ tapi tidak import unittest → bukan test.
//   13. Test-aware: entry point == test file → test detection skip.

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

    std::printf("=== frans_ai execute smoke test (Stage 7c + 7f.5) ===\n");
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
        std::printf("  exit=%d  success=%d  test_ran=%d\n",
                    ctx.lastExecution.exit_code,
                    ctx.success ? 1 : 0,
                    ctx.lastTest.ran ? 1 : 0);

        check(ctx.success, "ctx.success == true");
        check(ctx.lastExecution.exit_code == 0, "exit_code == 0");
        check(ctx.lastExecution.stdout_text == "4\n",
              "stdout == \"4\\n\"");
        check(!ctx.lastTest.ran, "no test file → lastTest.ran == false");
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

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastExecution.exit_code != 0, "exit_code != 0");
        check(ctx.lastExecution.stderr_text.find("ValueError")
                  != std::string::npos,
              "stderr mengandung 'ValueError'");
        check(!ctx.lastTest.ran, "no test file");
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

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastExecution.timed_out, "timed_out == true");
        check(ctx.lastExecution.duration_sec < 5.0, "duration < 5s");
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

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastError.find("BLOCKED") != std::string::npos,
              "lastError menyebut BLOCKED");
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

        check(!ctx.success, "ctx.success == false");
        check(ctx.lastError.find("tidak ada") != std::string::npos,
              "lastError menyebut file tidak ada");
    }

    // =====================================================================
    // Test 9 — Test-aware: test PASS + entry PASS
    // =====================================================================
    banner("Test-aware — test PASS + entry PASS");
    {
        const std::string tdir =
            (fs::temp_directory_path()
             / ("frans_7f5_pass_" + std::to_string(::getpid()))).string();
        fs::remove_all(tdir, ec);
        SandboxedFileSystem sb2(tdir);
        const std::string c2 = sb2.root().string();

        // Entry point: perkalian.py dengan __main__ block
        sb2.writeFile("perkalian.py",
            "def multiply(a, b):\n"
            "    return a * b\n"
            "\n"
            "if __name__ == '__main__':\n"
            "    print('The product is:', multiply(5, 3))\n");

        // Test file
        sb2.writeFile("test_perkalian.py",
            "import unittest\n"
            "from perkalian import multiply\n"
            "\n"
            "class TestPerkalian(unittest.TestCase):\n"
            "    def test_multiply(self):\n"
            "        self.assertEqual(multiply(5, 3), 15)\n"
            "        self.assertEqual(multiply(-2, 4), -8)\n"
            "        self.assertEqual(multiply(0, 5), 0)\n"
            "\n"
            "if __name__ == '__main__':\n"
            "    unittest.main()\n");

        AgentContext ctx;
        ctx.targetRelativePath = "perkalian.py";

        ExecuteState exec(c2, sb2, nullptr, 10);
        exec.run(ctx);

        std::printf("  lastTest: ran=%d file='%s' exit=%d dur=%.2fs\n",
                    ctx.lastTest.ran ? 1 : 0,
                    ctx.lastTest.test_file.c_str(),
                    ctx.lastTest.exit_code,
                    ctx.lastTest.duration_sec);
        std::printf("  lastExecution: exit=%d stdout='%s'\n",
                    ctx.lastExecution.exit_code,
                    ctx.lastExecution.stdout_text.c_str());

        check(ctx.lastTest.ran, "lastTest.ran == true");
        check(ctx.lastTest.test_file == "test_perkalian.py",
              "test_file == 'test_perkalian.py'");
        check(ctx.lastTest.exit_code == 0, "test exit_code == 0 (PASS)");
        check(ctx.lastExecution.exit_code == 0, "entry exit_code == 0");
        check(ctx.success, "ctx.success == true");
        check(ctx.lastExecution.stdout_text.find("The product is: 15")
                  != std::string::npos,
              "stdout entry mengandung 'The product is: 15'");
    }

    // =====================================================================
    // Test 10 — Test-aware: test FAIL → entry TIDAK dijalankan
    // =====================================================================
    banner("Test-aware — test FAIL → skip entry");
    {
        const std::string tdir =
            (fs::temp_directory_path()
             / ("frans_7f5_fail_" + std::to_string(::getpid()))).string();
        fs::remove_all(tdir, ec);
        SandboxedFileSystem sb2(tdir);
        const std::string c2 = sb2.root().string();

        // Entry point: perkalian.py dengan multiply BUGGY (a+b bukan a*b)
        sb2.writeFile("perkalian.py",
            "def multiply(a, b):\n"
            "    return a + b\n"   // bug: + bukan *
            "\n"
            "if __name__ == '__main__':\n"
            "    print('The product is:', multiply(5, 3))\n");

        sb2.writeFile("test_perkalian.py",
            "import unittest\n"
            "from perkalian import multiply\n"
            "\n"
            "class TestPerkalian(unittest.TestCase):\n"
            "    def test_multiply(self):\n"
            "        self.assertEqual(multiply(5, 3), 15)\n"
            "\n"
            "if __name__ == '__main__':\n"
            "    unittest.main()\n");

        AgentContext ctx;
        ctx.targetRelativePath = "perkalian.py";

        ExecuteState exec(c2, sb2, nullptr, 10);
        exec.run(ctx);

        std::printf("  lastTest: ran=%d exit=%d\n",
                    ctx.lastTest.ran ? 1 : 0, ctx.lastTest.exit_code);
        std::printf("  lastExecution exit=%d\n", ctx.lastExecution.exit_code);
        std::printf("  lastError='%s'\n", ctx.lastError.c_str());

        check(ctx.lastTest.ran, "lastTest.ran == true");
        check(ctx.lastTest.exit_code != 0, "test exit_code != 0 (FAIL)");
        check(!ctx.success, "ctx.success == false");
        check(ctx.lastError.find("Test failed") != std::string::npos,
              "lastError menyebut 'Test failed'");
        // lastExecution berisi hasil TEST, bukan entry.
        // Marker: stderr ada AssertionError dari unittest.
        check(ctx.lastExecution.stderr_text.find("AssertionError")
                  != std::string::npos,
              "lastExecution.stderr mengandung AssertionError "
              "(berarti test yang dieksekusi, bukan entry)");
        // Entry point tidak dijalankan: stdout tidak ada "The product is".
        check(ctx.lastExecution.stdout_text.find("The product is")
                  == std::string::npos,
              "stdout entry TIDAK ada (entry skipped)");
    }

    // =====================================================================
    // Test 11 — Test-aware: no test file → entry run saja
    // =====================================================================
    banner("Test-aware — no test file");
    {
        const std::string tdir =
            (fs::temp_directory_path()
             / ("frans_7f5_notest_" + std::to_string(::getpid()))).string();
        fs::remove_all(tdir, ec);
        SandboxedFileSystem sb2(tdir);
        const std::string c2 = sb2.root().string();

        sb2.writeFile("simple.py", "print('no test here')\n");

        AgentContext ctx;
        ctx.targetRelativePath = "simple.py";

        ExecuteState exec(c2, sb2, nullptr, 10);
        exec.run(ctx);

        check(!ctx.lastTest.ran, "lastTest.ran == false");
        check(ctx.lastExecution.exit_code == 0, "entry exit_code == 0");
        check(ctx.success, "ctx.success == true");
    }

    // =====================================================================
    // Test 12 — Test-aware: nama test_ tapi tidak import unittest
    // =====================================================================
    banner("Test-aware — test_*.py tanpa import unittest → bukan test");
    {
        const std::string tdir =
            (fs::temp_directory_path()
             / ("frans_7f5_fake_" + std::to_string(::getpid()))).string();
        fs::remove_all(tdir, ec);
        SandboxedFileSystem sb2(tdir);
        const std::string c2 = sb2.root().string();

        // Nama match test_ prefix, tapi isi bukan test file.
        sb2.writeFile("test_helpers.py",
            "def helper():\n"
            "    return 42\n");

        sb2.writeFile("main.py", "print('ok')\n");

        AgentContext ctx;
        ctx.targetRelativePath = "main.py";

        ExecuteState exec(c2, sb2, nullptr, 10);
        exec.run(ctx);

        check(!ctx.lastTest.ran,
              "lastTest.ran == false (bukan test file sebenarnya)");
        check(ctx.success, "ctx.success == true");
    }

    // =====================================================================
    // Test 13 — Test-aware: entry point == test file → skip test detection
    // =====================================================================
    banner("Test-aware — entry point == test file → run as entry saja");
    {
        const std::string tdir =
            (fs::temp_directory_path()
             / ("frans_7f5_same_" + std::to_string(::getpid()))).string();
        fs::remove_all(tdir, ec);
        SandboxedFileSystem sb2(tdir);
        const std::string c2 = sb2.root().string();

        // User set target = test file itu sendiri.
        sb2.writeFile("test_self.py",
            "import unittest\n"
            "\n"
            "class TestSelf(unittest.TestCase):\n"
            "    def test_ok(self):\n"
            "        self.assertTrue(True)\n"
            "\n"
            "if __name__ == '__main__':\n"
            "    print('running as entry')\n"
            "    unittest.main()\n");

        AgentContext ctx;
        ctx.targetRelativePath = "test_self.py";

        ExecuteState exec(c2, sb2, nullptr, 10);
        exec.run(ctx);

        check(!ctx.lastTest.ran,
              "lastTest.ran == false (test file = entry, tidak dijalankan "
              "dua kali)");
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    std::printf("sandbox tersisa di: %s\n", canonical.c_str());
    return g_failures == 0 ? 0 : 1;
}
