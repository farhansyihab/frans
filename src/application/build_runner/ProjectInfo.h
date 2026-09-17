// ~/AgenAI/frans/src/application/build_runner/ProjectInfo.h
#pragma once

#include <string>
#include <vector>

namespace frans::application {

// Jenis build system yang terdeteksi dari marker file di project root.
//
// PENTING: pemilihan build system ditentukan oleh MARKER FILE, bukan oleh
// ekstensi file yang di-import. File .cpp di project CMake → pakai CMake,
// BUKAN g++ manual. Ini yang membedakan desain baru dari desain lama
// (IBuildStrategy by language).
enum class BuildSystemKind {
    CMake,
    Make,
    Meson,
    Npm,        // package.json
    Poetry,     // pyproject.toml dengan [tool.poetry]
    Standalone, // tidak ada marker — fallback ke compiler/interpreter langsung
};

const char* buildSystemKindName(BuildSystemKind k);

// Info project yang terdeteksi oleh ProjectDetector.
//
// `sandbox_root` = folder yang user set di ManualImportDialog (tempat
//                  file ditulis).
// `project_root` = folder yang berisi marker build (CMakeLists.txt,
//                  package.json, dst). Bisa sama dengan sandbox_root,
//                  atau subfolder di bawahnya.
//
// SEMUA command build/test/run dijalankan dengan cwd = project_root,
// BUKAN sandbox_root dan BUKAN folder file individual.
struct ProjectInfo {
    std::string               sandbox_root;
    std::string               project_root;
    BuildSystemKind           kind           = BuildSystemKind::Standalone;
    std::string               test_runner;   // "CTest", "Pytest", dst.
                                             // Kosong = tidak ada test runner.
    std::string               primary_language; // "C++", "Python", dst (display).
    std::vector<std::string>  markers_found; // nama file marker yang ditemukan.

    // Path absolut + relative file yang di-import. Dipakai oleh
    // StandaloneBuildSystem yang perlu tahu entry point dari daftar file
    // (karena tidak ada marker yang memberitahu).
    std::vector<std::string>  imported_absolute_paths;
    std::vector<std::string>  imported_relative_paths;
};

} // namespace frans::application