// ~/AgenAI/frans/src/application/build_runner/BuildReport.h
#pragma once

#include <string>
#include <vector>

namespace frans::application {

// Blueprint Stage 11d §13.2 — satu file yang terkait dengan error build.
//
// `path` selalu relatif terhadap sandbox root.
//
// `role` nilainya:
//   "PRIMARY_ERROR"  — file + line yang men-trigger error (dari error parser)
//   "IMPORTS"        — file yang mengimpor primary_error (reverse dep, dari graph)
//   "DEPENDENT"      — file yang direferensikan primary_error (forward dep, dari graph)
struct RelatedFile {
    std::string path;
    int         line = 0;   // 1-based, 0 kalau tidak diketahui
    std::string role;
    std::string note;
};

// Hasil satu build/compile/run (Stage 11d).
//
// `success=true` ⇔ exit_code == 0 && !timed_out.
// Field `error_message` hanya terisi kalau success=false.
//
// REVISI (2026-01 follow-up):
//   `syntax_only=true` untuk C++ yang tidak punya `int main` — kita cek
//   sintaks saja (`g++ -fsyntax-only`), tidak compile+link.
//   Ini memberi user feedback sintaks tanpa harus jadi program standalone.
struct BuildReport {
    bool                        success        = false;
    int                         exit_code      = -1;
    bool                        timed_out      = false;

    // Stage 11d follow-up — flag ini di-set oleh BuildRunner ketika argv
    // mengandung `-fsyntax-only`. UI menampilkan catatan khusus supaya
    // user paham program tidak dijalankan.
    bool                        syntax_only    = false;

    std::string                 language;       // "Python" / "C++" / "TypeScript" / "JavaScript"
    std::string                 command;        // command yang dijalankan
    std::string                 entry_point;    // file yang di-compile/run
    std::string                 error_message;  // pesan error utama (kalau success=false)

    std::vector<RelatedFile>    related_files;  // primary + enriched (via graph)

    double                      duration_sec   = 0.0;
    std::string                 stdout_tail;
    std::string                 stderr_tail;
};

} // namespace frans::application