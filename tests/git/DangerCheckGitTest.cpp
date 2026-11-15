// tests/git/DangerCheckGitTest.cpp
//
// Fase D #69: Test pola git di DangerCheckHandler.
//
// Test tanpa FLTK, tanpa jaringan, tanpa git binary.
// Hanya test logika pattern matching untuk perintah git berbahaya.

#include "application/safety/DangerCheckHandler.h"
#include <cstdio>
#include <string>
#include <vector>

using frans::application::DangerCheckHandler;
using frans::application::DangerCheckResult;
using frans::application::DangerVerdict;

namespace {

int g_fails = 0;

void check(const char* label,
           const std::vector<std::string>& argv,
           DangerVerdict expected_verdict,
           const char* expected_pattern_substr = nullptr) {
    DangerCheckHandler h;
    const DangerCheckResult r = h.checkCommand(argv);

    bool pass = (r.verdict == expected_verdict);

    if (expected_pattern_substr && pass) {
        bool found = false;
        for (const auto& p : r.matched_patterns) {
            if (p.find(expected_pattern_substr) != std::string::npos) {
                found = true;
                break;
            }
        }
        if (!found) pass = false;
    }

    if (!pass) ++g_fails;

    const char* verdict_str =
        r.verdict == DangerVerdict::Safe         ? "Safe"         :
        r.verdict == DangerVerdict::NeedsConfirm ? "NeedsConfirm" :
                                                   "Blocked";

    std::printf("  %s %-52s verdict=%s",
                pass ? "[ok]  " : "[FAIL]",
                label,
                verdict_str);

    if (!pass) {
        std::printf("  patterns=[");
        for (std::size_t i = 0; i < r.matched_patterns.size(); ++i) {
            if (i) std::printf(", ");
            std::printf("'%s'", r.matched_patterns[i].c_str());
        }
        std::printf("]");
    }
    std::printf("\n");
}

} // namespace

int main() {
    std::printf("\n=== danger-check-git (Fase D #69) ===\n");
    std::printf("  Cek DangerCheckHandler::checkCommand untuk pola git.\n\n");

    // =====================================================================
    // git push
    // =====================================================================
    check("git push origin master (aman)",
          {"git", "push", "origin", "master"},
          DangerVerdict::Safe);

    check("git push --force",
          {"git", "push", "--force"},
          DangerVerdict::NeedsConfirm, "git:push");

    check("git push -f",
          {"git", "push", "-f"},
          DangerVerdict::NeedsConfirm, "git:push");

    check("git push --force-with-lease",
          {"git", "push", "--force-with-lease"},
          DangerVerdict::NeedsConfirm, "git:push");

    check("git push -f origin main",
          {"git", "push", "-f", "origin", "main"},
          DangerVerdict::NeedsConfirm, "git:push");

    check("git push --force origin master",
          {"git", "push", "--force", "origin", "master"},
          DangerVerdict::NeedsConfirm, "git:push");

    // =====================================================================
    // git reset
    // =====================================================================
    check("git reset --soft HEAD~1 (aman)",
          {"git", "reset", "--soft", "HEAD~1"},
          DangerVerdict::Safe);

    check("git reset --mixed HEAD (aman)",
          {"git", "reset", "--mixed", "HEAD"},
          DangerVerdict::Safe);

    check("git reset --hard",
          {"git", "reset", "--hard"},
          DangerVerdict::NeedsConfirm, "git:reset");

    check("git reset --hard HEAD~1",
          {"git", "reset", "--hard", "HEAD~1"},
          DangerVerdict::NeedsConfirm, "git:reset");

    check("git reset --hard origin/main",
          {"git", "reset", "--hard", "origin/main"},
          DangerVerdict::NeedsConfirm, "git:reset");

    // =====================================================================
    // git clean
    // =====================================================================
    check("git clean -n (dry run, aman)",
          {"git", "clean", "-n"},
          DangerVerdict::Safe);

    check("git clean -nd (dry run + dir, aman)",
          {"git", "clean", "-nd"},
          DangerVerdict::Safe);

    check("git clean -f",
          {"git", "clean", "-f"},
          DangerVerdict::NeedsConfirm, "git:clean");

    check("git clean --force",
          {"git", "clean", "--force"},
          DangerVerdict::NeedsConfirm, "git:clean");

    check("git clean -f -d",
          {"git", "clean", "-f", "-d"},
          DangerVerdict::NeedsConfirm, "git:clean");

    // =====================================================================
    // git checkout / git restore
    // =====================================================================
    check("git checkout master (aman)",
          {"git", "checkout", "master"},
          DangerVerdict::Safe);

    check("git checkout -b new-branch (aman)",
          {"git", "checkout", "-b", "new-branch"},
          DangerVerdict::Safe);

    check("git checkout .",
          {"git", "checkout", "."},
          DangerVerdict::NeedsConfirm, "git:checkout");

    check("git checkout -- .",
          {"git", "checkout", "--", "."},
          DangerVerdict::NeedsConfirm, "git:checkout");

    check("git checkout HEAD .",
          {"git", "checkout", "HEAD", "."},
          DangerVerdict::NeedsConfirm, "git:checkout");

    check("git restore .",
          {"git", "restore", "."},
          DangerVerdict::NeedsConfirm, "git:restore");

    check("git restore --staged .",
          {"git", "restore", "--staged", "."},
          DangerVerdict::NeedsConfirm, "git:restore");

    check("git restore src/main.cpp (aman)",
          {"git", "restore", "src/main.cpp"},
          DangerVerdict::Safe);

    check("git restore --staged src/main.cpp (aman)",
          {"git", "restore", "--staged", "src/main.cpp"},
          DangerVerdict::Safe);

    // =====================================================================
    // git config (credential)
    // =====================================================================
    check("git config user.name test (aman)",
          {"git", "config", "user.name", "test"},
          DangerVerdict::Safe);

    check("git config user.email a@b.com (aman)",
          {"git", "config", "user.email", "a@b.com"},
          DangerVerdict::Safe);

    check("git config credential.helper store",
          {"git", "config", "credential.helper", "store"},
          DangerVerdict::NeedsConfirm, "git:config");

    check("git config --global credential.helper cache",
          {"git", "config", "--global", "credential.helper", "cache"},
          DangerVerdict::NeedsConfirm, "git:config");

    check("git config --system credential.helper (uppercase test)",
          {"git", "config", "--system", "CREDENTIAL.helper", "x"},
          DangerVerdict::NeedsConfirm, "git:config");

    // =====================================================================
    // Non-git commands (tidak boleh trigger pola git)
    // =====================================================================
    check("echo git push --force (bukan git)",
          {"echo", "git", "push", "--force"},
          DangerVerdict::Safe);

    // ./git basename-nya adalah "git", jadi ini tetap terdeteksi sebagai git
    check("./git push -f (basename adalah 'git')",
          {"./git", "push", "-f"},
          DangerVerdict::NeedsConfirm, "git:push");

    check("/usr/bin/git push -f (absolute path)",
          {"/usr/bin/git", "push", "-f"},
          DangerVerdict::NeedsConfirm, "git:push");

    check("C:/Program Files/Git/cmd/git.exe push -f (Windows path)",
          {"C:/Program Files/Git/cmd/git.exe", "push", "-f"},
          DangerVerdict::NeedsConfirm, "git:push");

    // =====================================================================
    // Edge cases
    // =====================================================================
    check("git (tanpa subcommand, aman)",
          {"git"},
          DangerVerdict::Safe);

    check("git -C /tmp push -f (global flag sebelum subcmd)",
          {"git", "-C", "/tmp", "push", "-f"},
          DangerVerdict::NeedsConfirm, "git:push");

    check("git --git-dir=/x reset --hard (global flag sebelum subcmd)",
          {"git", "--git-dir=/x", "reset", "--hard"},
          DangerVerdict::NeedsConfirm, "git:reset");

    check("argv kosong",
          {},
          DangerVerdict::Safe);

    // =====================================================================
    // Summary
    // =====================================================================
    std::printf("\n  => %s\n", g_fails == 0 ? "PASS" : "FAIL");
    return g_fails == 0 ? 0 : 1;
}