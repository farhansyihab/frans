// ~/AgenAI/frans/src/application/build_runner/BuildReport.h
#pragma once

#include <string>
#include <vector>

namespace frans::application {

// Satu file yang terkait dengan error build/test.
//
// `path` selalu relatif terhadap project root.
//
// `role` nilainya:
//   "PRIMARY_ERROR"  — file + line yang men-trigger error (dari error parser)
//   "IMPORTS"        — file yang mengimpor primary_error (reverse dep)
//   "DEPENDENT"      — file yang direferensikan primary_error (forward dep)
struct RelatedFile {
    std::string path;
    int         line = 0;   // 1-based, 0 kalau tidak diketahui
    std::string role;
    std::string note;
};

// Phase eksekusi. Urutannya: Build → Test → Run.
// Test di-skip kalau tidak ada test runner terdeteksi.
// Run di-skip kalau tidak ada entry point terdeteksi.
enum class BuildPhase {
    Build,
    Test,
    Run,
};

const char* buildPhaseName(BuildPhase p);

// Hasil satu command dalam satu phase. Satu phase bisa terdiri dari
// beberapa command (mis. CMake: `cmake -S . -B build` lalu `cmake --build build`).
struct CommandResult {
    std::string label;             // "configure", "compile", "test", "run"
    std::string command;           // argv yang di-join, untuk display
    bool        ran         = true;
    bool        success     = false;
    int         exit_code   = -1;
    bool        timed_out   = false;
    bool        cancelled   = false;
    double      duration_sec = 0.0;
    std::string stdout_tail;
    std::string stderr_tail;
};

// Hasil satu phase (Build / Test / Run).
struct PhaseResult {
    BuildPhase  phase       = BuildPhase::Build;
    bool        skipped     = true;   // true = tidak ada command untuk phase ini
    bool        success     = false;
    double      duration_sec = 0.0;
    std::string error_message;        // pesan error utama (kalau success=false)
    std::vector<RelatedFile> related_files;
    std::vector<CommandResult> commands;

    bool ran() const { return !skipped; }
};

// Hasil satu panggilan BuildRunner::runBuild().
struct BuildReport {
    // ---- Identifikasi project (dari ProjectDetector) ----
    std::string project_root;
    std::string sandbox_root;
    std::string build_system;      // "CMake" | "Make" | "Meson" | "npm" | "Poetry" | "Standalone"
    std::string test_runner;       // "CTest" | "Pytest" | dst. Kosong = tidak ada.
    std::string primary_language;  // "C++" | "Python" | dst.

    // ---- Hasil per phase ----
    PhaseResult build;
    PhaseResult test;
    PhaseResult run;

    // ---- Ringkasan ----
    bool        overall_success   = false;
    double      total_duration_sec = 0.0;
    BuildPhase  failed_phase      = BuildPhase::Build; // valid kalau !overall_success

    // Helper: phase mana yang dijalankan (tidak skipped).
    bool has_any_phase_ran() const {
        return build.ran() || test.ran() || run.ran();
    }

    // Backwards-compat accessor untuk UI lama.
    bool is_success() const { return overall_success; }
};

} // namespace frans::application