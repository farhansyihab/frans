// test_proc_win.cpp — HAPUS setelah selesai test §9.2 #21.
//
// Test ProcessExecutor_win32.cpp:
//   1. Command sederhana + stdout capture
//   2. working_dir (chdir)
//   3. timeout (SIGKILL analog di Windows = TerminateProcess)
//   4. cancel_flag (user-initiated)
//   5. exit code propagation

#include "infrastructure/system/ProcessExecutor.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

using namespace frans::infrastructure;

int main() {
    int pass = 0, fail = 0;

    // ---- Test 1: command sederhana ----
    {
        std::printf("\n[TEST 1] cmd /c echo hello\n");
        auto r = runCommand({"cmd", "/c", "echo hello"}, 10);
        std::printf("  exit=%d  timed_out=%d  cancelled=%d\n",
                    r.exit_code, r.timed_out, r.cancelled_by_user);
        std::printf("  stdout='%s'\n", r.stdout_text.c_str());
        std::printf("  stderr='%s'\n", r.stderr_text.c_str());
        if (r.exit_code == 0
            && r.stdout_text.find("hello") != std::string::npos) {
            std::printf("  [OK]\n"); ++pass;
        } else {
            std::printf("  [FAIL]\n"); ++fail;
        }
    }

    // ---- Test 2: working_dir ----
    {
        std::printf("\n[TEST 2] working_dir = C:\\Windows\n");
        auto r = runCommand({"cmd", "/c", "cd"}, 10, "", "C:\\Windows");
        std::printf("  stdout='%s'\n", r.stdout_text.c_str());
        if (r.stdout_text.find("Windows") != std::string::npos) {
            std::printf("  [OK]\n"); ++pass;
        } else {
            std::printf("  [FAIL] cwd tidak diterapkan\n"); ++fail;
        }
    }

    // ---- Test 3: timeout ----
    {
        std::printf("\n[TEST 3] timeout 2s (ping sleep ~9s)\n");
        auto t0 = std::chrono::steady_clock::now();
        auto r = runCommand({"cmd", "/c", "ping -n 10 127.0.0.1 > nul"}, 2);
        auto dt = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        std::printf("  exit=%d  timed_out=%d  elapsed=%.1fs\n",
                    r.exit_code, r.timed_out, dt);
        if (r.timed_out && dt < 5.0) {
            std::printf("  [OK]\n"); ++pass;
        } else {
            std::printf("  [FAIL]\n"); ++fail;
        }
    }

    // ---- Test 4: cancel_flag ----
    {
        std::printf("\n[TEST 4] cancel_flag setelah 1.5s\n");
        std::atomic<bool> cancel{false};
        std::thread killer([&]{
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            cancel.store(true);
        });
        auto t0 = std::chrono::steady_clock::now();
        auto r = runCommand({"cmd", "/c", "ping -n 10 127.0.0.1 > nul"},
                            30, "", "", &cancel);
        auto dt = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        killer.join();
        std::printf("  exit=%d  cancelled=%d  elapsed=%.1fs\n",
                    r.exit_code, r.cancelled_by_user, dt);
        if (r.cancelled_by_user && dt < 5.0) {
            std::printf("  [OK]\n"); ++pass;
        } else {
            std::printf("  [FAIL]\n"); ++fail;
        }
    }

    // ---- Test 5: exit code non-zero ----
    {
        std::printf("\n[TEST 5] cmd /c exit 42\n");
        auto r = runCommand({"cmd", "/c", "exit 42"}, 10);
        std::printf("  exit=%d\n", r.exit_code);
        if (r.exit_code == 42) {
            std::printf("  [OK]\n"); ++pass;
        } else {
            std::printf("  [FAIL] expected 42\n"); ++fail;
        }
    }

    std::printf("\n=== ProcessExecutor Summary: %d passed, %d failed ===\n",
                pass, fail);
    return fail == 0 ? 0 : 1;
}