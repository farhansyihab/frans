// ~/AgenAI/frans/tests/build_runner_smoke_test.cpp
//
// Smoke test Stage 11d: BuildRunner.
//
// Skenario:
//   1. detectDominantLanguage           — sanity check.
//   2. Empty files_written              — BuildReport error.
//   3. Python sukses                    — main.py valid -> SUCCESS.
//   4. Python syntax error              — `def f(` -> FAILED + related.
//   5. Python runtime error             — ZeroDivisionError -> FAILED + related.
//   6. Vue — expected to fail cleanly   — pesan "Vue" jelas.
//   7. C++ standalone dengan main       — full build sukses (a.out dibuat).
//   8. C++ tanpa main                   — syntax check sukses (exit 0).
//   9. C++ dengan include lokal         — include path heuristic membantu.
//  10. C++ dengan header tidak ada      — gagal dengan pesan yang jelas.
//
// Jalankan: ./frans_build_runner_test [sandbox_root]
// Default sandbox_root = /tmp/frans_build_runner_test_<pid>

#include "application/build_runner/BuildRunner.h"
#include "application/agent/MultiFileParser.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

using frans::application::BuildLanguage;
using frans::application::BuildReport;
using frans::application::BuildRunner;
using frans::application::GeneratedFile;
using frans::infrastructure::SandboxedFileSystem;

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& label) {
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", label.c_str());
    if (!cond) ++g_failures;
}

void writeRaw(const fs::path& root, const std::string& rel,
              const std::string& content)
{
    const fs::path target = root / rel;
    fs::create_directories(target.parent_path());
    std::ofstream f(target, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

GeneratedFile makeGenerated(const std::string& path, const std::string& content) {
    GeneratedFile gf;
    gf.relative_path = path;
    gf.content       = content;
    return gf;
}

} // namespace

int main(int argc, char** argv) {
    std::string root;
    if (argc > 1) {
        root = argv[1];
    } else {
        root = (fs::temp_directory_path()
                / ("frans_build_runner_test_" + std::to_string(::getpid())))
                .string();
    }

    std::printf("=== frans_build_runner smoke test (Stage 11d) ===\n");
    std::printf("root: %s\n\n", root.c_str());

    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    SandboxedFileSystem sb(root);
    BuildRunner runner;

    // =====================================================================
    // Test 1: detectDominantLanguage
    // =====================================================================
    std::printf("[Test 1] detectDominantLanguage\n");
    {
        std::vector<GeneratedFile> py = {
            makeGenerated("a.py", ""),
            makeGenerated("b.py", ""),
        };
        check(BuildRunner::detectDominantLanguage(py) == BuildLanguage::Python,
              "2x .py -> Python");

        std::vector<GeneratedFile> mixed = {
            makeGenerated("a.py", ""),
            makeGenerated("b.cpp", ""),
            makeGenerated("c.cpp", ""),
        };
        check(BuildRunner::detectDominantLanguage(mixed) == BuildLanguage::Cpp,
              "1x .py + 2x .cpp -> C++");

        std::vector<GeneratedFile> empty;
        check(BuildRunner::detectDominantLanguage(empty) == BuildLanguage::Unknown,
              "empty -> Unknown");

        std::vector<GeneratedFile> vue = {
            makeGenerated("a.vue", ""),
        };
        check(BuildRunner::detectDominantLanguage(vue) == BuildLanguage::Vue,
              "1x .vue -> Vue");
    }

    // =====================================================================
    // Test 2: empty files_written
    // =====================================================================
    std::printf("\n[Test 2] empty files_written\n");
    {
        std::vector<GeneratedFile> empty;
        const BuildReport r = runner.runBuild(
            empty, root, sb, /*graph_cache=*/nullptr);
        check(!r.success, "success=false");
        check(!r.error_message.empty(), "error_message terisi");
    }

    // =====================================================================
    // Test 3: Python sukses (hello world)
    // =====================================================================
    std::printf("\n[Test 3] Python sukses\n");
    {
        const std::string content =
            "def hello():\n"
            "    return \"Hello\"\n"
            "\n"
            "if __name__ == \"__main__\":\n"
            "    print(hello())\n";
        writeRaw(root, "main.py", content);

        std::vector<GeneratedFile> files = {
            makeGenerated("main.py", content),
        };

        const BuildReport r = runner.runBuild(
            files, root, sb, /*graph_cache=*/nullptr);

        check(r.success,                "success=true");
        check(r.exit_code == 0,         "exit_code=0");
        check(r.language == "Python",   "language=Python");
        check(r.entry_point == "main.py", "entry_point=main.py");
        check(!r.command.empty(),       "command terisi");
        check(!r.syntax_only,           "syntax_only=false");
        check(r.related_files.empty(),  "related_files kosong (sukses)");
    }

    // =====================================================================
    // Test 4: Python syntax error
    // =====================================================================
    std::printf("\n[Test 4] Python syntax error\n");
    {
        const std::string content =
            "def hello(\n"
            "    return \"Hello\"\n";
        writeRaw(root, "broken.py", content);

        std::vector<GeneratedFile> files = {
            makeGenerated("broken.py", content),
        };

        const BuildReport r = runner.runBuild(
            files, root, sb, /*graph_cache=*/nullptr);

        check(!r.success,             "success=false");
        check(r.exit_code != 0,       "exit_code != 0");
        check(r.language == "Python", "language=Python");
        check(!r.error_message.empty(), "error_message terisi");

        bool has_primary = false;
        for (const auto& rf : r.related_files) {
            if (rf.role == "PRIMARY_ERROR") has_primary = true;
        }
        check(has_primary, "related_files punya PRIMARY_ERROR");
    }

    // =====================================================================
    // Test 5: Python runtime error (ZeroDivisionError)
    // =====================================================================
    std::printf("\n[Test 5] Python runtime error\n");
    {
        const std::string content =
            "def divide(a, b):\n"
            "    return a / b\n"
            "\n"
            "if __name__ == \"__main__\":\n"
            "    print(divide(1, 0))\n";
        writeRaw(root, "runtime.py", content);

        std::vector<GeneratedFile> files = {
            makeGenerated("runtime.py", content),
        };

        const BuildReport r = runner.runBuild(
            files, root, sb, /*graph_cache=*/nullptr);

        check(!r.success,             "success=false");
        check(r.exit_code != 0,       "exit_code != 0");
        check(r.error_message.find("ZeroDivisionError") != std::string::npos,
              "error_message menyebut ZeroDivisionError");
    }

    // =====================================================================
    // Test 6: Vue — tidak didukung
    // =====================================================================
    std::printf("\n[Test 6] Vue — expected to fail cleanly\n");
    {
        std::vector<GeneratedFile> files = {
            makeGenerated("Component.vue", "<template></template>"),
        };

        const BuildReport r = runner.runBuild(
            files, root, sb, /*graph_cache=*/nullptr);

        check(!r.success, "success=false");
        check(r.error_message.find("Vue") != std::string::npos,
              "error_message menyebut Vue");
    }

    // =====================================================================
    // Test 7: C++ standalone dengan `int main` — full build
    // =====================================================================
    std::printf("\n[Test 7] C++ standalone dengan main\n");
    {
        const std::string content =
            "#include <iostream>\n"
            "\n"
            "int main() {\n"
            "    std::cout << \"hello from cpp\" << std::endl;\n"
            "    return 0;\n"
            "}\n";
        writeRaw(root, "hello.cpp", content);

        std::vector<GeneratedFile> files = {
            makeGenerated("hello.cpp", content),
        };

        const BuildReport r = runner.runBuild(
            files, root, sb, /*graph_cache=*/nullptr);

        check(r.success,             "success=true");
        check(r.exit_code == 0,      "exit_code=0");
        check(r.language == "C++",   "language=C++");
        check(!r.syntax_only,        "syntax_only=false (full build)");
        check(r.command.find("-fsyntax-only") == std::string::npos,
              "command tidak punya -fsyntax-only");
        check(r.command.find("-o a.out") != std::string::npos,
              "command punya -o a.out");
    }

    // =====================================================================
    // Test 8: C++ tanpa main — syntax check only
    // =====================================================================
    std::printf("\n[Test 8] C++ tanpa main -> syntax-only\n");
    {
        const std::string content =
            "#include <string>\n"
            "\n"
            "std::string greet(const std::string& n) {\n"
            "    return \"hi \" + n;\n"
            "}\n";
        writeRaw(root, "lib.cpp", content);

        std::vector<GeneratedFile> files = {
            makeGenerated("lib.cpp", content),
        };

        const BuildReport r = runner.runBuild(
            files, root, sb, /*graph_cache=*/nullptr);

        check(r.success,             "success=true (syntax OK)");
        check(r.exit_code == 0,      "exit_code=0");
        check(r.syntax_only,         "syntax_only=true");
        check(r.command.find("-fsyntax-only") != std::string::npos,
              "command punya -fsyntax-only");
        check(r.command.find("-o a.out") == std::string::npos,
              "command tidak punya -o a.out");
    }

    // =====================================================================
    // Test 9: C++ dengan include lokal — include path heuristic
    // =====================================================================
    std::printf("\n[Test 9] C++ dengan include lokal (via -I <root>)\n");
    {
        // helper.h di root, main.cpp di subdir — main.cpp include
        // "helper.h" yang resolve lewat `-I <root>`.
        const std::string helper =
            "#pragma once\n"
            "inline int answer() { return 42; }\n";
        const std::string main_cpp =
            "#include \"helper.h\"\n"
            "#include <iostream>\n"
            "\n"
            "int main() {\n"
            "    std::cout << answer() << std::endl;\n"
            "    return 0;\n"
            "}\n";

        writeRaw(root, "helper.h", helper);
        writeRaw(root, "main.cpp", main_cpp);

        std::vector<GeneratedFile> files = {
            makeGenerated("helper.h", helper),
            makeGenerated("main.cpp", main_cpp),
        };

        const BuildReport r = runner.runBuild(
            files, root, sb, /*graph_cache=*/nullptr);

        check(r.success,           "success=true (include resolve)");
        check(!r.syntax_only,      "syntax_only=false (ada main)");
    }

    // =====================================================================
    // Test 10: C++ dengan header yang tidak ada — gagal dengan pesan jelas
    // =====================================================================
    std::printf("\n[Test 10] C++ dengan header tidak ada\n");
    {
        const std::string content =
            "#include \"nonexistent_header_xyz.h\"\n"
            "\n"
            "int main() { return 0; }\n";
        writeRaw(root, "bad_include.cpp", content);

        std::vector<GeneratedFile> files = {
            makeGenerated("bad_include.cpp", content),
        };

        const BuildReport r = runner.runBuild(
            files, root, sb, /*graph_cache=*/nullptr);

        check(!r.success, "success=false");
        // Pesan error harus menyebut header yang tidak ditemukan
        // (dari g++). Fallback line juga harus terisi.
        check(!r.error_message.empty(),
              "error_message terisi (g++ fallback line)");
        const bool mentions_missing =
            r.error_message.find("nonexistent_header_xyz") != std::string::npos
         || r.stderr_tail.find("nonexistent_header_xyz") != std::string::npos;
        check(mentions_missing, "error menyebut nama header yang hilang");
    }

    std::printf("\n=== %s ===\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    std::printf("sandbox tersisa di: %s\n", sb.root().string().c_str());
    return g_failures == 0 ? 0 : 1;
}