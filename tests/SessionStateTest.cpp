// tests/SessionStateTest.cpp
//
// Test otomatis Fase A #44 + #45 (BluePrint v9 Addendum §4.8).
//
// Yang di-test (semua murni, tanpa FLTK / tanpa subprocess):
//   1. frans::application::summarizeBuildError()
//   2. frans::application::wasCancelledByUser()
//   3. Aturan SessionState yang dipakai ManualImportDialog::onBuildPoll:
//        gagal   -> publish
//        sukses  -> clear
//        cancel  -> JANGAN ubah
//        session nullptr -> tidak crash
//   4. Integrasi ke PromptGenerator: ringkasan masuk ke blok {{#ERROR}}.
//
// Jalankan:
//   cmake -S . -B build -DFRANS_BUILD_TESTS=ON
//   cmake --build build -j2
//   ./build/bin/frans_session_tests
//
// Catatan: fungsi applyBuildOutcome() di bawah adalah CERMIN dari cabang
// keputusan ManualImportDialog::onBuildPoll(). Kalau logika di dialog
// diubah, cermin ini harus ikut diubah (lihat tests/README-FaseA-44.md).

#include "application/build_runner/BuildErrorSummary.h"
#include "application/prompt/PromptGenerator.h"
#include "ui/SessionState.h"

#include <cstdio>
#include <string>

namespace {

using frans::application::BuildPhase;
using frans::application::BuildReport;
using frans::application::CommandResult;
using frans::application::PhaseResult;
using frans::application::RelatedFile;
using frans::application::summarizeBuildError;
using frans::application::wasCancelledByUser;
using frans::ui::SessionState;

int g_pass = 0;
int g_fail = 0;

void beginTest(const char* name, const char* desc) {
    std::printf("\n=== %s ===\n  %s\n", name, desc);
}

void checkContains(const std::string& hay, const std::string& needle,
                   const char* label)
{
    const bool ok = hay.find(needle) != std::string::npos;
    if (ok) ++g_pass; else ++g_fail;
    std::printf("  %s %-34s contains '%s'\n",
                ok ? "[ok]  " : "[FAIL]", label, needle.c_str());
    if (!ok) std::printf("        actual: '%s'\n", hay.c_str());
}

void checkNotContains(const std::string& hay, const std::string& needle,
                      const char* label)
{
    const bool ok = hay.find(needle) == std::string::npos;
    if (ok) ++g_pass; else ++g_fail;
    std::printf("  %s %-34s NOT contains '%s'\n",
                ok ? "[ok]  " : "[FAIL]", label, needle.c_str());
    if (!ok) std::printf("        actual: '%s'\n", hay.c_str());
}

void checkTrue(bool cond, const char* label) {
    if (cond) ++g_pass; else ++g_fail;
    std::printf("  %s %s\n", cond ? "[ok]  " : "[FAIL]", label);
}

// ---------------------------------------------------------------------------
// Fixture BuildReport
// ---------------------------------------------------------------------------

CommandResult failedCommand(const std::string& label,
                            const std::string& cmd,
                            const std::string& stderr_text,
                            const std::string& stdout_text = {})
{
    CommandResult cr;
    cr.label       = label;
    cr.command     = cmd;
    cr.ran         = true;
    cr.success     = false;
    cr.exit_code   = 1;
    cr.stderr_tail = stderr_text;
    cr.stdout_tail = stdout_text;
    return cr;
}

// CMake: compile error C++ pada src/main.cpp:7.
BuildReport makeCppBuildFailure() {
    BuildReport r;
    r.project_root     = "/tmp/frans_a44_cmake";
    r.sandbox_root     = r.project_root;
    r.build_system     = "CMake";
    r.primary_language = "C++";
    r.overall_success  = false;
    r.failed_phase     = BuildPhase::Build;

    r.build.phase   = BuildPhase::Build;
    r.build.skipped = false;
    r.build.success = false;
    r.build.error_message = "'undeclared_variable' was not declared in this scope";

    RelatedFile rf;
    rf.path = "src/main.cpp";
    rf.line = 7;
    rf.role = "PRIMARY_ERROR";
    rf.note = "'undeclared_variable' was not declared in this scope";
    r.build.related_files.push_back(rf);

    r.build.commands.push_back(failedCommand(
        "compile", "cmake --build build -j2",
        "/tmp/frans_a44_cmake/src/main.cpp:7:10: error: "
        "'undeclared_variable' was not declared in this scope\n"));

    r.test.phase   = BuildPhase::Test;
    r.test.skipped = true;
    r.test.success = true;
    r.run.phase    = BuildPhase::Run;
    r.run.skipped  = true;
    r.run.success  = true;
    return r;
}

// CTest gagal di phase TEST (build sukses).
BuildReport makeTestFailure() {
    BuildReport r;
    r.project_root     = "/tmp/frans_a44_cmake";
    r.sandbox_root     = r.project_root;
    r.build_system     = "CMake";
    r.test_runner      = "CTest";
    r.primary_language = "C++";
    r.overall_success  = false;
    r.failed_phase     = BuildPhase::Test;

    r.build.phase   = BuildPhase::Build;
    r.build.skipped = false;
    r.build.success = true;

    r.test.phase   = BuildPhase::Test;
    r.test.skipped = false;
    r.test.success = false;
    r.test.error_message = "1 of 1 tests failed";
    r.test.commands.push_back(failedCommand(
        "ctest", "ctest --test-dir build --output-on-failure",
        "The following tests FAILED:\n\t  1 - demo_run (Failed)\n"));

    r.run.phase   = BuildPhase::Run;
    r.run.skipped = true;
    r.run.success = true;
    return r;
}

// Python traceback di phase RUN.
BuildReport makePythonRunFailure() {
    BuildReport r;
    r.project_root     = "/tmp/frans_a44_py";
    r.sandbox_root     = r.project_root;
    r.build_system     = "Standalone";
    r.primary_language = "Python";
    r.overall_success  = false;
    r.failed_phase     = BuildPhase::Run;

    r.build.phase   = BuildPhase::Build;
    r.build.skipped = true;
    r.build.success = true;
    r.test.phase    = BuildPhase::Test;
    r.test.skipped  = true;
    r.test.success  = true;

    r.run.phase   = BuildPhase::Run;
    r.run.skipped = false;
    r.run.success = false;
    r.run.error_message = "ValueError: sengaja gagal untuk uji Fase A #44";

    RelatedFile rf;
    rf.path = "app.py";
    rf.line = 9;
    rf.role = "PRIMARY_ERROR";
    rf.note = "python error";
    r.run.related_files.push_back(rf);

    r.run.commands.push_back(failedCommand(
        "run", "python3 app.py",
        "Traceback (most recent call last):\n"
        "  File \"/tmp/frans_a44_py/app.py\", line 9, in <module>\n"
        "    main()\n"
        "ValueError: sengaja gagal untuk uji Fase A #44\n"));
    return r;
}

BuildReport makeSuccess() {
    BuildReport r;
    r.project_root     = "/tmp/frans_a44_py";
    r.sandbox_root     = r.project_root;
    r.build_system     = "Standalone";
    r.primary_language = "Python";
    r.overall_success  = true;

    r.build.phase   = BuildPhase::Build;
    r.build.skipped = true;
    r.build.success = true;
    r.test.phase    = BuildPhase::Test;
    r.test.skipped  = true;
    r.test.success  = true;
    r.run.phase     = BuildPhase::Run;
    r.run.skipped   = false;
    r.run.success   = true;
    return r;
}

BuildReport makeCancelled() {
    BuildReport r = makePythonRunFailure();
    r.run.error_message = "Dibatalkan user pada run";
    r.run.related_files.clear();
    r.run.commands.clear();

    CommandResult cr;
    cr.label            = "run";
    cr.command          = "python3 sleeper.py";
    cr.success          = false;
    cr.cancelled        = true;
    cr.exit_code        = 137;
    cr.stdout_tail      = "Mulai tidur 60 detik...\n";
    r.run.commands.push_back(cr);
    return r;
}

// ---------------------------------------------------------------------------
// Cermin aturan ManualImportDialog::onBuildPoll (Fase A #44).
// ---------------------------------------------------------------------------
void applyBuildOutcome(SessionState* session, const BuildReport& report) {
    if (!session) return;                       // dialog tanpa session pointer
    if (wasCancelledByUser(report)) return;     // cancel: jangan timpa
    if (report.overall_success) {
        session->last_build_error.clear();      // sukses: bersihkan
    } else {
        session->last_build_error = summarizeBuildError(report);
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testSessionDefaults() {
    beginTest("session-defaults", "SessionState baru harus kosong semua.");
    SessionState s;
    checkTrue(s.last_project_root.empty(), "last_project_root kosong");
    checkTrue(s.last_build_error.empty(),  "last_build_error kosong");
    checkTrue(s.last_context_path.empty(), "last_context_path kosong");
}

void testSummarizeCppBuildFailure() {
    beginTest("summarize-build-fail-cpp",
              "Build CMake gagal: phase, build system, error, path:line, stderr.");
    const std::string s = summarizeBuildError(makeCppBuildFailure());
    checkContains(s, "Build FAILED pada phase BUILD", "header phase");
    checkContains(s, "(CMake)",                      "build system");
    checkContains(s, "Error: 'undeclared_variable'", "error_message");
    checkContains(s, "Related files:",               "related header");
    checkContains(s, "src/main.cpp:7",               "path:line");
    checkContains(s, "stderr tail:",                 "stderr header");
    checkContains(s, "src/main.cpp:7:10: error",     "stderr isi");
    checkNotContains(s, "phase TEST",                "bukan phase TEST");
}

void testSummarizeTestFailure() {
    beginTest("summarize-test-fail",
              "Build sukses, CTest gagal -> ringkasan menunjuk phase TEST.");
    const std::string s = summarizeBuildError(makeTestFailure());
    checkContains(s, "phase TEST",              "header phase TEST");
    checkContains(s, "1 of 1 tests failed",     "error_message test");
    checkContains(s, "demo_run (Failed)",       "stderr ctest");
    checkNotContains(s, "phase BUILD",          "bukan phase BUILD");
}

void testSummarizeRunFailure() {
    beginTest("summarize-run-fail-python",
              "Python traceback di phase RUN -> app.py:9 ikut terbawa.");
    const std::string s = summarizeBuildError(makePythonRunFailure());
    checkContains(s, "phase RUN",           "header phase RUN");
    checkContains(s, "(Standalone)",        "build system");
    checkContains(s, "ValueError",          "error_message");
    checkContains(s, "app.py:9",            "path:line");
    checkContains(s, "Traceback",           "stderr traceback");
}

void testSummarizeMinimal() {
    beginTest("summarize-minimal",
              "Gagal tanpa related_files / tanpa command -> tetap aman.");
    BuildReport r;
    r.build_system    = "Make";
    r.overall_success = false;
    r.failed_phase    = BuildPhase::Build;
    r.build.phase     = BuildPhase::Build;
    r.build.skipped   = false;
    r.build.success   = false;

    const std::string s = summarizeBuildError(r);
    checkContains(s, "Build FAILED pada phase BUILD", "header tetap ada");
    checkContains(s, "(Make)",                        "build system");
    checkNotContains(s, "Related files:",             "tanpa related");
    checkNotContains(s, "stderr tail:",               "tanpa stderr");
}

void testSummarizeTruncation() {
    beginTest("summarize-stderr-tail-2kb",
              "stderr panjang dipotong dari belakang (default 2048 byte).");
    BuildReport r = makeCppBuildFailure();
    std::string big(8 * 1024, 'x');
    big += "BARIS-TERAKHIR";
    r.build.commands.clear();
    r.build.commands.push_back(
        failedCommand("compile", "cmake --build build", big));

    const std::string s = summarizeBuildError(r);
    checkContains(s, "...(truncated)...", "penanda truncated");
    checkContains(s, "BARIS-TERAKHIR",    "ekor stderr dipertahankan");
    checkTrue(s.size() < 3000, "ukuran ringkasan < 3000 byte");

    const std::string small = summarizeBuildError(r, 64);
    checkTrue(small.size() < s.size(), "max_stderr_bytes=64 lebih pendek");
    checkContains(small, "BARIS-TERAKHIR", "ekor tetap ada (custom max)");
}

void testSummarizeStdoutOnly() {
    beginTest("summarize-stdout-only",
              "Command gagal tanpa stderr -> tidak menulis bagian stderr.");
    BuildReport r = makePythonRunFailure();
    r.run.commands.clear();
    r.run.commands.push_back(failedCommand(
        "run", "python3 app.py", "", "output di stdout saja\n"));

    const std::string s = summarizeBuildError(r);
    checkContains(s, "phase RUN",        "header tetap ada");
    checkContains(s, "app.py:9",         "related tetap ada");
    checkNotContains(s, "stderr tail:",  "tanpa bagian stderr");
}

void testSummarizeSuccessEmpty() {
    beginTest("summarize-success-empty",
              "Report sukses -> ringkasan kosong (tidak ada error).");
    const std::string s = summarizeBuildError(makeSuccess());
    checkTrue(s.empty(), "summarizeBuildError(sukses) == kosong");
}

void testCancelDetection() {
    beginTest("cancel-detection",
              "Cancel dikenali dari flag CommandResult maupun pesan.");
    checkTrue(wasCancelledByUser(makeCancelled()),        "flag cancelled");
    checkTrue(!wasCancelledByUser(makeCppBuildFailure()), "gagal biasa != cancel");
    checkTrue(!wasCancelledByUser(makeSuccess()),         "sukses != cancel");

    BuildReport r = makePythonRunFailure();
    r.run.commands.clear();
    r.run.error_message = "Dibatalkan user sebelum command run";
    checkTrue(wasCancelledByUser(r), "fallback pesan 'Dibatalkan'");
}

void testSessionRules() {
    beginTest("session-publish-clear-cancel",
              "Aturan dialog: gagal=publish, sukses=clear, cancel=abaikan.");
    SessionState s;

    applyBuildOutcome(&s, makeCppBuildFailure());
    checkTrue(!s.last_build_error.empty(), "gagal -> last_build_error terisi");
    checkContains(s.last_build_error, "src/main.cpp:7", "isi dari build gagal");

    // Cancel tidak boleh menimpa error sebelumnya.
    const std::string before = s.last_build_error;
    applyBuildOutcome(&s, makeCancelled());
    checkTrue(s.last_build_error == before, "cancel -> error lama dipertahankan");

    // Error baru (phase RUN) menimpa error lama.
    applyBuildOutcome(&s, makePythonRunFailure());
    checkContains(s.last_build_error, "phase RUN", "error baru menimpa");
    checkNotContains(s.last_build_error, "src/main.cpp:7", "error lama hilang");

    // Sukses membersihkan.
    applyBuildOutcome(&s, makeSuccess());
    checkTrue(s.last_build_error.empty(), "sukses -> last_build_error kosong");

    // Session null tidak boleh crash (backwards compatible).
    applyBuildOutcome(nullptr, makeCppBuildFailure());
    checkTrue(true, "session nullptr -> tidak crash");
}

void testPromptIntegration() {
    beginTest("prompt-integration-45",
              "Ringkasan #44 masuk ke blok {{#ERROR}} pada prompt (#45).");
    const std::string summary = summarizeBuildError(makeCppBuildFailure());

    frans::application::PromptInput in;
    in.tmpl         = frans::application::defaultPromptTemplate();
    in.request      = "Perbaiki error compile berikut.";
    in.error_output = summary;

    const std::string out = frans::application::renderPrompt(in);
    checkContains(out, "--- error output ---",   "blok ERROR dirender");
    checkContains(out, "Perbaiki error compile", "request masuk");
    checkContains(out, "phase BUILD",            "phase ikut ke prompt");
    checkContains(out, "src/main.cpp:7",         "path:line ikut ke prompt");
    checkNotContains(out, "{{ERROR_OUTPUT}}",    "placeholder terganti");
    checkNotContains(out, "{{#ERROR}}",          "tag blok hilang");

    frans::application::PromptInput clean;
    clean.tmpl    = frans::application::defaultPromptTemplate();
    clean.request = "Tambah fitur baru.";
    const std::string out2 = frans::application::renderPrompt(clean);
    checkNotContains(out2, "--- error output ---", "tanpa error: blok dilewati");
}

} // namespace

int main() {
    std::printf("Frans -- test Fase A #44 / #45 (SessionState + BuildErrorSummary)\n");

    testSessionDefaults();
    testSummarizeCppBuildFailure();
    testSummarizeTestFailure();
    testSummarizeRunFailure();
    testSummarizeMinimal();
    testSummarizeTruncation();
    testSummarizeStdoutOnly();
    testSummarizeSuccessEmpty();
    testCancelDetection();
    testSessionRules();
    testPromptIntegration();

    std::printf("\n=== Summary: %d checks passed, %d failed ===\n",
                g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}