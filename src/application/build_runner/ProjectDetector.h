// ~/AgenAI/frans/src/application/build_runner/ProjectDetector.h
#pragma once

#include "application/agent/MultiFileParser.h"   // GeneratedFile
#include "application/build_runner/ProjectInfo.h"

#include <string>
#include <vector>

namespace frans::application {

// Stage 11d-revisi — deteksi project root + build system.
//
// Alur (sesuai blueprint step 1-3):
//
//   Step 1 — Cari project root:
//     Mulai dari direktori setiap file yang di-import, walk UP ke parent
//     sampai ketemu marker build (CMakeLists.txt, package.json, dst),
//     atau sampai di sandbox_root (fallback).
//
//   Step 2 — Pilih build system:
//     Berdasarkan marker file yang ditemukan di project_root.
//     Prioritas: CMake > Meson > Make > Npm > Poetry > Standalone.
//
//   Step 3 — Cari test runner:
//     Cek marker test (enable_testing di CMakeLists.txt, scripts.test di
//     package.json, conftest.py / pytest.ini untuk pytest, dst).
//
// Deterministik, murni filesystem query. TIDAK menulis apapun.
class ProjectDetector {
public:
    // Deteksi project root + build system + test runner.
    //
    // `files_written` — file yang baru di-commit via ManualImporter.
    // `sandbox_root`  — absolute path sandbox root (dari UI).
    //
    // Return ProjectInfo dengan semua field terisi. Kalau tidak ada marker
    // ditemukan, `project_root` = sandbox_root dan `kind` = Standalone.
    static ProjectInfo detect(
        const std::vector<GeneratedFile>& files_written,
        const std::string& sandbox_root);
};

} // namespace frans::application