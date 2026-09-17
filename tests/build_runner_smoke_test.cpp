// ~/AgenAI/frans/tests/build_runner_smoke_test.cpp
//
// Smoke test untuk BuildRunner + ProjectDetector (Stage 11d-revisi).
//
// Cakupan:
//   1. ProjectDetector: deteksi project root + build system dari marker file
//      (CMakeLists.txt → CMake, package.json → Npm, tanpa marker → Standalone,
//       subfolder → project_root = subfolder).
//   2. Standalone Python: run sukses.
//   3. Standalone Python: syntax error → gagal di phase Run.
//   4. Standalone Python: runtime error (ZeroDivisionError).
//   5. Standalone C++: full build (ada main).
//   6. Standalone C++: syntax-only (tidak ada main).
//   7. Empty files → gagal.
//
// Test tidak butuh network / toolchain eksternal (cmake tidak dipanggil).
// Hanya butuh: python3, g++ (atau fallback yang sesuai).
#include "application/build_runner/BuildRunner.h"
#include "application/build_runner/ProjectDetector.h"
#include "application/build_runner/ProjectInfo.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

using frans::application::BuildPhase;
using frans::application::BuildReport;
using frans::application::BuildRunner;
using frans::application::BuildSystemKind;
using frans::application::GeneratedFile;
using frans::application::ProjectDetector;
using frans::application::ProjectInfo;
using frans::infrastructure::SandboxedFileSystem;

// ---------------------------------------------------------------------------
// Test harness minimal.
// ---------------------------------------------------------------------------
static int g_failures = 0;
static int g_total    = 0;

static void check(bool cond, const char* msg) {
    ++g_total;
    if (cond) {
        std::printf("  [PASS] %s\n", msg);
    } else {
        std::printf("  [FAIL] %s\n", msg);
        ++g_failures;
    }
}

// ---------------------------------------------------------------------------
// Helper: buat temp dir bersih (hapus kalau sudah ada).
// ---------------------------------------------------------------------------
static std::string makeTempDir(const char* suffix) {
    std::error_code ec;
    fs::path base = fs::temp_directory_path(ec) / "frans_br_test";
    if (ec) base = "/tmp/frans_br_test";
    base /= suffix;

    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    return base.string();
}

// Helper: tulis file di luar sandbox (untuk fixture sebelum sandbox dibuat).
static void writeFixtureFile(const std::string& root,
                              const std::string& rel,
                              const std::string& content)
{
    fs::path full = fs::path(root) / rel;
    std::error_code ec;
    fs::create_directories(full.parent_path(), ec);
    std::ofstream f(full, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

static GeneratedFile makeGF(const std::string& rel, const std::string& content) {
    GeneratedFile gf;
    gf.relative_path = rel;
    gf.content       = content;
    return gf;
}

// ---------------------------------------------------------------------------
int main() {
    // =====================================================================
    // Test 1 — ProjectDetector: CMake marker terdeteksi di root.
    // =====================================================================
    {
        std::printf("\n[Test 1] ProjectDetector: CMake marker\n");
        const std::string root = makeTempDir("cmake");
        writeFixtureFile(root, "CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(x CXX)\n"
            "add_executable(x main.cpp)\n");
        writeFixtureFile(root, "main.cpp",
            "int main() { return 0; }\n");

        std::vector<GeneratedFile> files = {
            makeGF("main.cpp", "int main() { return 0; }\n")
        };

        const ProjectInfo info = ProjectDetector::detect(files, root);

        check(info.kind == BuildSystemKind::CMake,      "kind == CMake");
        check(info.project_root == root,                "project_root == sandbox_root");
        check(info.primary_language == "C++",           "primary_language == C++");
        check(info.markers_found.size() == 1,           "1 marker ditemukan");
    }

    // =====================================================================
    // Test 2 — ProjectDetector: package.json → Npm + NpmTest.
    // =====================================================================
    {
        std::printf("\n[Test 2] ProjectDetector: package.json\n");
        const std::string root = makeTempDir("npm");
        writeFixtureFile(root, "package.json",
            "{\"name\":\"x\",\"scripts\":{\"test\":\"echo ok\"}}\n");
        writeFixtureFile(root, "index.js", "console.log('hi');\n");

        std::vector<GeneratedFile> files = {
            makeGF("index.js", "console.log('hi');\n")
        };

        const ProjectInfo info = ProjectDetector::detect(files, root);

        check(info.kind == BuildSystemKind::Npm,        "kind == Npm");
        check(info.test_runner == "NpmTest",            "test_runner == NpmTest");
    }

    // =====================================================================
    // Test 3 — ProjectDetector: tanpa marker → Standalone.
    // =====================================================================
    {
        std::printf("\n[Test 3] ProjectDetector: tanpa marker\n");
        const std::string root = makeTempDir("standalone");
        std::vector<GeneratedFile> files = {
            makeGF("main.py", "print('hello')\n")
        };

        const ProjectInfo info = ProjectDetector::detect(files, root);

        check(info.kind == BuildSystemKind::Standalone, "kind == Standalone");
        check(info.project_root == root,                "project_root == sandbox_root");
        check(info.primary_language == "Python",        "primary_language == Python");
    }

    // =====================================================================
    // Test 4 — ProjectDetector: file di subfolder, marker di root.
    //          project_root harus walk-up ke folder yang punya marker.
    // =====================================================================
    {
        std::printf("\n[Test 4] ProjectDetector: subfolder walk-up\n");
        const std::string root = makeTempDir("walkup");
        writeFixtureFile(root, "CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(y CXX)\n");
        writeFixtureFile(root, "src/util.cpp", "int helper(){return 1;}\n");

        std::vector<GeneratedFile> files = {
            makeGF("src/util.cpp", "int helper(){return 1;}\n")
        };

        const ProjectInfo info = ProjectDetector::detect(files, root);

        check(info.kind == BuildSystemKind::CMake,      "kind == CMake");
        check(info.project_root == root,                "project_root == root (walk-up)");
    }

    // =====================================================================
    // Test 5 — Standalone Python: run sukses.
    // =====================================================================
    {
        std::printf("\n[Test 5] Standalone Python: run sukses\n");
        const std::string root = makeTempDir("py_ok");
        SandboxedFileSystem fs_impl(root);

        const std::string content = "print('hello world')\n";
        fs_impl.writeFile("main.py", content);

        std::vector<GeneratedFile> files = { makeGF("main.py", content) };
        BuildRunner runner;
        const BuildReport r = runner.runBuild(files, root, fs_impl, nullptr);

        check(r.overall_success,                  "overall_success == true");
        check(r.primary_language == "Python",     "primary_language == Python");
        check(r.build_system == "Standalone",     "build_system == Standalone");
        check(r.run.success,                      "run.success == true");
        check(r.run.commands.size() == 1,         "1 command di phase run");
    }

    // =====================================================================
    // Test 6 — Standalone Python: runtime error (ZeroDivisionError).
    // =====================================================================
    {
        std::printf("\n[Test 6] Standalone Python: runtime error\n");
        const std::string root = makeTempDir("py_runtime_err");
        SandboxedFileSystem fs_impl(root);

        const std::string content = "x = 1\ny = 0\nprint(x / y)\n";
        fs_impl.writeFile("main.py", content);

        std::vector<GeneratedFile> files = { makeGF("main.py", content) };
        BuildRunner runner;
        const BuildReport r = runner.runBuild(files, root, fs_impl, nullptr);

        check(!r.overall_success,                 "overall_success == false");
        check(r.failed_phase == BuildPhase::Run,  "failed_phase == Run");
        check(r.run.error_message.find("ZeroDivisionError") != std::string::npos,
              "error_message mengandung ZeroDivisionError");
    }

    // =====================================================================
    // Test 7 — Standalone C++: full build (ada main).
    // =====================================================================
    {
        std::printf("\n[Test 7] Standalone C++: full build\n");
        const std::string root = makeTempDir("cpp_main");
        SandboxedFileSystem fs_impl(root);

        const std::string content =
            "#include <iostream>\n"
            "int main() { std::cout << \"hello\" << std::endl; return 0; }\n";
        fs_impl.writeFile("main.cpp", content);

        std::vector<GeneratedFile> files = { makeGF("main.cpp", content) };
        BuildRunner runner;
        const BuildReport r = runner.runBuild(files, root, fs_impl, nullptr);

        check(r.overall_success,                  "overall_success == true");
        check(r.primary_language == "C++",        "primary_language == C++");
        check(r.build.success,                    "build.success == true");

        // Command build harus mengandung "-o a.out" (bukan -fsyntax-only).
        bool has_o = false;
        for (const auto& cmd : r.build.commands) {
            if (cmd.command.find("-o a.out") != std::string::npos) {
                has_o = true;
                break;
            }
        }
        check(has_o,                              "build command mengandung -o a.out");
    }

    // =====================================================================
    // Test 8 — Standalone C++: syntax-only (tidak ada main).
    // =====================================================================
    {
        std::printf("\n[Test 8] Standalone C++: syntax-only\n");
        const std::string root = makeTempDir("cpp_no_main");
        SandboxedFileSystem fs_impl(root);

        const std::string content = "int helper() { return 42; }\n";
        fs_impl.writeFile("util.cpp", content);

        std::vector<GeneratedFile> files = { makeGF("util.cpp", content) };
        BuildRunner runner;
        const BuildReport r = runner.runBuild(files, root, fs_impl, nullptr);

        check(r.overall_success,                  "overall_success == true");
        check(r.build.success,                    "build.success == true");

        bool has_syntax_only = false;
        for (const auto& cmd : r.build.commands) {
            if (cmd.command.find("-fsyntax-only") != std::string::npos) {
                has_syntax_only = true;
                break;
            }
        }
        check(has_syntax_only,                    "build command mengandung -fsyntax-only");
        check(r.run.skipped,                      "run.skipped == true (tidak ada main)");
    }

    // =====================================================================
    // Test 9 — Empty files → error.
    // =====================================================================
    {
        std::printf("\n[Test 9] Empty files\n");
        const std::string root = makeTempDir("empty");
        SandboxedFileSystem fs_impl(root);

        std::vector<GeneratedFile> files;
        BuildRunner runner;
        const BuildReport r = runner.runBuild(files, root, fs_impl, nullptr);

        check(!r.overall_success,                       "overall_success == false");
        check(r.failed_phase == BuildPhase::Build,      "failed_phase == Build");
        check(!r.build.error_message.empty(),           "build.error_message terisi");
    }

    // =====================================================================
    // Test 10 — CMake project: deteksi CTest dari enable_testing().
    //           Tidak dijalankan (butuh toolchain cmake), cukup deteksi.
    // =====================================================================
    {
        std::printf("\n[Test 10] CMake project: deteksi CTest\n");
        const std::string root = makeTempDir("cmake_ctest");
        writeFixtureFile(root, "CMakeLists.txt",
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(hello CXX)\n"
            "enable_testing()\n"
            "add_executable(hello main.cpp)\n"
            "add_test(NAME hello_test COMMAND hello)\n");
        writeFixtureFile(root, "main.cpp",
            "#include <iostream>\n"
            "int main(){ std::cout << \"hi\"; return 0; }\n");

        std::vector<GeneratedFile> files = {
            makeGF("main.cpp",
                "#include <iostream>\n"
                "int main(){ std::cout << \"hi\"; return 0; }\n")
        };

        const ProjectInfo info = ProjectDetector::detect(files, root);

        check(info.kind == BuildSystemKind::CMake,      "kind == CMake");
        check(info.test_runner == "CTest",              "test_runner == CTest");
    }

    // =====================================================================
    std::printf("\n=== %d/%d checks passed ===\n",
                g_total - g_failures, g_total);
    return g_failures == 0 ? 0 : 1;
}