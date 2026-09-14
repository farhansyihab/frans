// ~/AgenAI/frans/tests/diff_engine_smoke_test.cpp
//
// Smoke test Stage 7g: DiffEngine.
// Tidak butuh llama-server — pure C++.

#include "infrastructure/system/DiffEngine.h"

#include <cstdio>
#include <string>

using namespace frans::infrastructure;

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
    std::printf("=== frans_ai diff engine smoke test (Stage 7g) ===\n");

    // =====================================================================
    // Test 1 — identical content
    // =====================================================================
    banner("Identical content");
    {
        const auto d = computeDiff("x.py", "a\nb\nc\n", "a\nb\nc\n");
        check(d.added_count == 0,   "added == 0");
        check(d.removed_count == 0, "removed == 0");
        check(!d.is_new_file,       "not new file");
        check(!d.is_deleted,        "not deleted");
        check(d.lines.size() == 3,  "3 unchanged lines");
    }

    // =====================================================================
    // Test 2 — all removed
    // =====================================================================
    banner("All removed");
    {
        const auto d = computeDiff("x.py", "a\nb\nc\n", "");
        check(d.added_count == 0,   "added == 0");
        check(d.removed_count == 3, "removed == 3");
        check(!d.is_new_file,       "not new file");
        check(d.is_deleted,         "is deleted");
    }

    // =====================================================================
    // Test 3 — new file
    // =====================================================================
    banner("New file (old empty)");
    {
        const auto d = computeDiff("x.py", "", "a\nb\n");
        check(d.added_count == 2, "added == 2");
        check(d.removed_count == 0, "removed == 0");
        check(d.is_new_file, "is new file");
        check(!d.is_deleted, "not deleted");
    }

    // =====================================================================
    // Test 4 — mixed edit (1 line changed)
    // =====================================================================
    banner("Mixed edit (1 line changed)");
    {
        const auto d = computeDiff("x.py", "a\nb\nc\n", "a\nX\nc\n");
        check(d.added_count >= 1,   "added >= 1");
        check(d.removed_count >= 1, "removed >= 1");
        check(!d.is_new_file,       "not new file");
        check(!d.is_deleted,        "not deleted");
        // Harus ada minimal 1 Added (X) dan 1 Removed (b).
        bool has_added_x   = false;
        bool has_removed_b = false;
        for (const auto& l : d.lines) {
            if (l.kind == DiffLine::Kind::Added   && l.text == "X") has_added_x   = true;
            if (l.kind == DiffLine::Kind::Removed && l.text == "b") has_removed_b = true;
        }
        check(has_added_x,   "has added line X");
        check(has_removed_b, "has removed line b");
    }

    // =====================================================================
    // Test 5 — add lines only
    // =====================================================================
    banner("Add lines only");
    {
        const auto d = computeDiff("x.py", "a\nb\n", "a\nb\nc\nd\n");
        check(d.added_count == 2, "added == 2");
        check(d.removed_count == 0, "removed == 0");
    }

    // =====================================================================
    // Test 6 — format output
    // =====================================================================
    banner("formatDiff output");
    {
        const auto d = computeDiff("hello.py",
                                    "print('old')\n",
                                    "print('new')\n");
        const auto s = formatDiff(d);
        std::printf("--- output ---\n%s---\n", s.c_str());
        check(s.find("=== hello.py ===") != std::string::npos, "header present");
        check(s.find("-") != std::string::npos, "'-' prefix present");
        check(s.find("+") != std::string::npos, "'+' prefix present");
    }

    // =====================================================================
    // Test 7 — format no-changes
    // =====================================================================
    banner("formatDiff no-changes");
    {
        const auto d = computeDiff("x.py", "a\n", "a\n");
        const auto s = formatDiff(d);
        check(s.find("(no changes)") != std::string::npos,
              "shows (no changes)");
    }

    // =====================================================================
    // Test 8 — format new file tag
    // =====================================================================
    banner("formatDiff new file tag");
    {
        const auto d = computeDiff("x.py", "", "hello\n");
        const auto s = formatDiff(d);
        check(s.find("[NEW FILE]") != std::string::npos,
              "[NEW FILE] tag present");
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
