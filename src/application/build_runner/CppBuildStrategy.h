// ~/AgenAI/frans/src/application/build_runner/CppBuildStrategy.h
#pragma once

#include "application/build_runner/IBuildStrategy.h"

namespace frans::application {

// Strategy build untuk C++ (Stage 11d, revisited 2026-01 follow-up).
//
// Entry point detection:
//   1. File .cpp/.cc/.cxx yang content-nya mengandung "int main(" atau
//      "int main (".
//   2. Fallback: file .cpp/.cc/.cxx pertama (urut alfabetis). Fallback
//      ini penting supaya file non-standalone tetap bisa di-syntax-check.
//
// Build mode (dua mode, dipilih otomatis):
//
//   A. FULL BUILD — kalau ADA file dengan `int main` di antara yang diimpor.
//        g++ -std=c++20 -I<root> -I<root>/src -I<root>/include
//            -o a.out <semua .cpp/.cc/.cxx>
//      Compile + link jadi executable `a.out`. Cocok untuk program
//      standalone (mis. `hello.cpp`).
//
//   B. SYNTAX CHECK ONLY — kalau TIDAK ADA `int main`.
//        g++ -std=c++20 -I<root> -I<root>/src -I<root>/include
//            -fsyntax-only <semua .cpp/.cc/.cxx>
//      Parse + cek sintaks, tidak link, tidak hasilkan `a.out`.
//      Berguna kalau user meng-import file yang bagian dari project besar
//      (mis. `ManualImportDialog.cpp`) — ingin tahu sintaks benar.
//
// Include path yang selalu ditambahkan (kalau sandbox_root non-empty):
//   -I <sandbox_root>           (umum)
//   -I <sandbox_root>/src       (project layout frans)
//   -I <sandbox_root>/include   (project layout umum lain)
//
// Header eksternal (mis. `<FL/Fl_Window.H>`) TIDAK di-supply. Kalau file
// butuh header eksternal, syntax check akan gagal dengan pesan
// "No such file or directory" dari g++ — ini ekspektasi, karena build
// project lengkap dengan dependensi eksternal harus lewat cmake/make.
//
// Error parsing:
//   Format GCC/Clang: `path:line:col: error|warning|note: message`.
//   Match pertama dengan severity "error" = lokasi error utama.
class CppBuildStrategy : public IBuildStrategy {
public:
    std::string languageName() const override;

    std::string detectEntryPoint(
        const std::vector<GeneratedFile>& files,
        frans::domain::IFileSystemPort& fs,
        const std::string& sandbox_root) override;

    std::vector<std::string> buildCommand(
        const std::string& entry_point,
        const std::vector<GeneratedFile>& files,
        const std::string& sandbox_root) override;

    BuildReport parseOutput(
        int exit_code,
        bool timed_out,
        const std::string& stdout_text,
        const std::string& stderr_text,
        double duration_sec,
        const std::string& sandbox_root) override;
};

} // namespace frans::application