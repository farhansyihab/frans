// tests/ScenarioTest.cpp
//
// Runner test scenario untuk Manual Import + Danger Check + Sandbox
// + error parser (traceback frame picking).
//
// Mode:
//   frans_scenarios              -> jalankan semua scenario, print PASS/FAIL
//   frans_scenarios --list       -> daftar nama + deskripsi
//   frans_scenarios --dump NAME  -> print isi scenario (paste ke GUI)
//
// Scenario yang di-cover (sesuai Blueprint §9.2 + §9.3):
//   §9.2:
//     01 python-standalone        -> #16
//     02 cmake-multifile          -> regression
//     03 path-escape              -> #18 (parser)
//     04 danger-python            -> #19
//     05 ai-browser-realistic     -> #17
//     06 cancel-sleep             -> #23 (manual via GUI)
//   §9.3:
//     07 npm-basic                -> #22
//     08 npm-typescript           -> #22
//     09 standalone-js            -> #22
//   plus:
//     sandbox-path-safety         -> #18 (sandbox)
//     traceback-frame-picking     -> regression (anomali 1)

#include "application/import/MultiFileParser.h"
#include "application/safety/DangerCheckHandler.h"
#include "application/build_runner/BuildReport.h"
#include "application/build_runner/ProjectInfo.h"
#include "application/build_runner/BuildSystems/StandaloneBuildSystem.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

using frans::application::parseModelOutput;
using frans::application::BuildPhase;
using frans::application::BuildSystemKind;
using frans::application::CommandResult;
using frans::application::DangerCheckHandler;
using frans::application::DangerVerdict;
using frans::application::PhaseResult;
using frans::application::ProjectInfo;
using frans::application::StandaloneBuildSystem;
using frans::domain::GeneratedFile;

// Hindari literal triple-backtick di source (bisa mengganggu tooling).
// Placeholder "###F###" di-expand menjadi tiga backtick oleh expandFences().
const std::string kFence              = "`" "`" "`";
const std::string kFencePlaceholder   = "###F###";

std::string expandFences(const std::string& raw) {
    std::string result;
    result.reserve(raw.size() + 32);
    std::size_t pos = 0;
    while (pos < raw.size()) {
        const std::size_t hit = raw.find(kFencePlaceholder, pos);
        if (hit == std::string::npos) {
            result.append(raw, pos, std::string::npos);
            break;
        }
        result.append(raw, pos, hit - pos);
        result += kFence;
        pos = hit + kFencePlaceholder.size();
    }
    return result;
}

enum class DangerExpect {
    Any,
    Safe,
    NeedsConfirm,
    Blocked,
};

struct FileExpect {
    std::string  relative_path;
    DangerExpect danger = DangerExpect::Any;
};

struct Scenario {
    std::string             name;
    std::string             description;
    std::string             raw_input;          // pakai ###F### untuk fence
    std::size_t             expected_file_count = 0;
    bool                    expect_parse_error  = false;
    std::vector<FileExpect> file_expects;
};

std::vector<Scenario> buildScenarios() {
    std::vector<Scenario> v;

    // ---- 01: Python standalone ----
    {
        Scenario s;
        s.name = "python-standalone";
        s.description = "Skrip Python tanpa marker build system.";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: hello.py
###F###python
#!/usr/bin/env python3
"""Contoh skrip Python standalone."""

def greet(name: str) -> str:
    return f"Halo, {name}!"

def main() -> None:
    print(greet("Dunia"))

if __name__ == "__main__":
    main()
###F###
)RAW";
        s.file_expects.push_back({"hello.py", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 02: CMake multi-file (regression §9.1 #12) ----
    {
        Scenario s;
        s.name = "cmake-multifile";
        s.description = "CMakeLists.txt + src/main.cpp (2 file).";
        s.expected_file_count = 2;
        s.raw_input = R"RAW(File: CMakeLists.txt
###F###cmake
cmake_minimum_required(VERSION 3.16)
project(demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(demo src/main.cpp)

enable_testing()
add_test(NAME demo_run COMMAND demo)
###F###

File: src/main.cpp
###F###cpp
#include <iostream>

int main() {
    std::cout << "Halo dari demo\n";
    return 0;
}
###F###
)RAW";
        s.file_expects.push_back({"CMakeLists.txt", DangerExpect::Safe});
        s.file_expects.push_back({"src/main.cpp",    DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 03: Path escape (parser accept, sandbox reject) ----
    {
        Scenario s;
        s.name = "path-escape";
        s.description = "File dengan '../' — parser accept, sandbox harus "
                        "BLOCKED (lihat sandbox-path-safety).";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: ../evil.py
###F###python
print("coba keluar sandbox")
###F###
)RAW";
        s.file_expects.push_back({"../evil.py", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 04: Danger pattern Python ----
    {
        Scenario s;
        s.name = "danger-python";
        s.description = "os.system + shutil.rmtree — harus NeedsConfirm.";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: cleanup.py
###F###python
import os
import shutil

def wipe(path):
    os.system("rm -rf " + path)
    shutil.rmtree(path)

if __name__ == "__main__":
    wipe("/tmp/scratch")
###F###
)RAW";
        s.file_expects.push_back({"cleanup.py", DangerExpect::NeedsConfirm});
        v.push_back(std::move(s));
    }

    // ---- 05: Output AI browser realistis ----
    {
        Scenario s;
        s.name = "ai-browser-realistic";
        s.description = "Output dengan prose + multi-file, seperti DeepSeek.";
        s.expected_file_count = 2;
        s.raw_input = R"RAW(Berikut adalah implementasi yang kamu minta.
Saya buat dua file: satu module utility, satu entry point.

File: utils.py
###F###python
"""Utility sederhana."""

def tambah(a, b):
    return a + b

def kurang(a, b):
    return a - b
###F###

File: app.py
###F###python
from utils import tambah, kurang

def main():
    print(tambah(2, 3))
    print(kurang(10, 4))

if __name__ == "__main__":
    main()
###F###

Semoga membantu! Kalau ada error, kabari saya.
)RAW";
        s.file_expects.push_back({"utils.py", DangerExpect::Safe});
        s.file_expects.push_back({"app.py",   DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 06: Cancel test (§9.3 #23) — manual via GUI ----
    {
        Scenario s;
        s.name = "cancel-sleep";
        s.description = "Script Python sleep 60 detik. Untuk test Cancel "
                        "button: paste, Write, Build, klik Cancel dalam "
                        "2 detik, verify status CANCELLED.";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: sleeper.py
###F###python
import time

def main():
    print("Mulai tidur 60 detik...", flush=True)
    time.sleep(60)
    print("Selesai tidur", flush=True)

if __name__ == "__main__":
    main()
###F###
)RAW";
        s.file_expects.push_back({"sleeper.py", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 07: npm basic (§9.3 #22) ----
    {
        Scenario s;
        s.name = "npm-basic";
        s.description = "Project npm sederhana: package.json + src/index.js. "
                        "Uji: npm install -> npm run build -> npm test.";
        s.expected_file_count = 2;
        s.raw_input = R"RAW(File: package.json
###F###json
{
  "name": "demo-npm-basic",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "build": "node --check src/index.js",
    "test": "node --check src/index.js"
  }
}
###F###

File: src/index.js
###F###javascript
function greet(name) {
  return "Halo, " + name + "!";
}

console.log(greet("npm"));
###F###
)RAW";
        s.file_expects.push_back({"package.json",   DangerExpect::Safe});
        s.file_expects.push_back({"src/index.js",   DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 08: npm TypeScript (§9.3 #22) ----
    {
        Scenario s;
        s.name = "npm-typescript";
        s.description = "Project npm dengan TypeScript. Uji: npm install "
                        "(tarik tsc) -> tsc --noEmit via npm run build.";
        s.expected_file_count = 3;
        s.raw_input = R"RAW(File: package.json
###F###json
{
  "name": "demo-npm-typescript",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "build": "tsc --noEmit",
    "test": "tsc --noEmit"
  },
  "devDependencies": {
    "typescript": "^5.4.0"
  }
}
###F###

File: tsconfig.json
###F###json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "commonjs",
    "strict": true,
    "noEmit": true,
    "esModuleInterop": true,
    "skipLibCheck": true
  },
  "include": ["src/**/*.ts"]
}
###F###

File: src/index.ts
###F###typescript
function greet(name: string): string {
  return "Halo, " + name + "!";
}

const msg: string = greet("TypeScript");
console.log(msg);
###F###
)RAW";
        s.file_expects.push_back({"package.json",  DangerExpect::Safe});
        s.file_expects.push_back({"tsconfig.json", DangerExpect::Safe});
        s.file_expects.push_back({"src/index.ts",  DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    // ---- 09: Standalone JavaScript (§9.3 #22) ----
    {
        Scenario s;
        s.name = "standalone-js";
        s.description = "JS tanpa package.json. Uji: node --check "
                        "(build) + node hello.js (run).";
        s.expected_file_count = 1;
        s.raw_input = R"RAW(File: hello.js
###F###javascript
function greet(name) {
  return "Halo, " + name + "!";
}

console.log(greet("Dunia"));
###F###
)RAW";
        s.file_expects.push_back({"hello.js", DangerExpect::Safe});
        v.push_back(std::move(s));
    }

    return v;
}

const char* dangerName(DangerVerdict v) {
    switch (v) {
        case DangerVerdict::Safe:         return "Safe";
        case DangerVerdict::NeedsConfirm: return "NeedsConfirm";
        case DangerVerdict::Blocked:      return "Blocked";
    }
    return "?";
}

bool dangerMatches(DangerVerdict actual, DangerExpect expected) {
    if (expected == DangerExpect::Any) return true;
    if (expected == DangerExpect::Safe         && actual == DangerVerdict::Safe)         return true;
    if (expected == DangerExpect::NeedsConfirm && actual == DangerVerdict::NeedsConfirm) return true;
    if (expected == DangerExpect::Blocked      && actual == DangerVerdict::Blocked)      return true;
    return false;
}

std::string languageFromPath(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return {};
    const std::string ext = path.substr(dot + 1);
    if (ext == "py")  return "python";
    if (ext == "cpp" || ext == "cc" || ext == "cxx"
     || ext == "hpp" || ext == "hxx" || ext == "hh" || ext == "h") return "cpp";
    if (ext == "ts"  || ext == "tsx") return "typescript";
    if (ext == "js"  || ext == "jsx" || ext == "mjs" || ext == "cjs") return "javascript";
    if (ext == "vue") return "vue";
    return {};
}

bool runScenario(const Scenario& s) {
    std::printf("\n=== %s ===\n  %s\n",
                s.name.c_str(), s.description.c_str());

    const std::string input = expandFences(s.raw_input);
    const auto parse = parseModelOutput(input, "");

    bool ok = true;

    if (s.expect_parse_error) {
        if (parse.error.empty()) {
            std::printf("  [FAIL] expected parse error, tapi parse sukses\n");
            ok = false;
        } else {
            std::printf("  [ok] parse error seperti diharapkan: %s\n",
                        parse.error.c_str());
        }
    } else if (!parse.error.empty()) {
        std::printf("  [FAIL] parse error: %s\n", parse.error.c_str());
        ok = false;
    }

    if (s.expected_file_count > 0
        && parse.files.size() != s.expected_file_count) {
        std::printf("  [FAIL] expected %zu file, got %zu\n",
                    s.expected_file_count, parse.files.size());
        ok = false;
    } else if (s.expected_file_count > 0) {
        std::printf("  [ok] %zu file di-parse\n", parse.files.size());
    }

    DangerCheckHandler danger;

    for (std::size_t i = 0; i < parse.files.size(); ++i) {
        const auto& f = parse.files[i];
        const std::string lang = languageFromPath(f.relative_path);
        const auto dc = danger.checkCode(f.content, lang);

        std::printf("    file[%zu] %-24s lang=%-10s danger=%s\n",
                    i, f.relative_path.c_str(),
                    lang.empty() ? "?" : lang.c_str(),
                    dangerName(dc.verdict));

        if (i < s.file_expects.size()) {
            const auto& exp = s.file_expects[i];
            if (!exp.relative_path.empty()
                && exp.relative_path != f.relative_path) {
                std::printf("      [FAIL] path expected '%s', got '%s'\n",
                            exp.relative_path.c_str(),
                            f.relative_path.c_str());
                ok = false;
            }
            if (!dangerMatches(dc.verdict, exp.danger)) {
                std::printf("      [FAIL] danger expected != actual\n");
                ok = false;
            }
        }
    }

    std::printf("  => %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ---- Test sandbox (path containment) — terpisah dari scenario ----

bool runSandboxTest() {
    std::printf("\n=== sandbox-path-safety ===\n");
    std::printf("  Cek SandboxedFileSystem::isPathSafe() — path escape "
                "harus ditolak.\n");

    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path tmp = fs::temp_directory_path(ec);
    if (ec) {
        std::printf("  [SKIP] temp_directory_path gagal: %s\n",
                    ec.message().c_str());
        return true;
    }
    tmp /= "frans_test_sandbox_v8";

    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    if (ec) {
        std::printf("  [SKIP] create_directories gagal: %s\n",
                    ec.message().c_str());
        return true;
    }

    frans::infrastructure::SandboxedFileSystem sb(tmp.string());

    int fails = 0;
    auto check = [&](const char* p, bool expected_safe) {
        const bool actual = sb.isPathSafe(p);
        const bool pass = (actual == expected_safe);
        if (!pass) ++fails;
        std::printf("  %s isPathSafe(%-28s) = %s (harus %s)\n",
                    pass ? "[ok]  " : "[FAIL]",
                    p,
                    actual ? "true" : "false",
                    expected_safe ? "true" : "false");
    };

    check("hello.py",          true);
    check("src/main.cpp",      true);
    check("a/b/c/d.py",        true);

    check("../evil.py",        false);
    check("../../etc/passwd",  false);
    check("/etc/passwd",       false);
    check("src/../../evil.py", false);

    fs::remove_all(tmp, ec);

    const bool ok = (fails == 0);
    std::printf("  => %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ---- Test parseOutput: traceback frame picking (anomali 1) ----

bool runTracebackPickingTest() {
    std::printf("\n=== traceback-frame-picking ===\n");
    std::printf("  Cek StandaloneBuildSystem::parseOutput memilih frame "
                "PERTAMA di dalam project_root (file user), bukan frame "
                "stdlib di akhir traceback.\n");

    PhaseResult pr;
    pr.phase   = BuildPhase::Run;
    pr.skipped = false;
    pr.success = false;

    CommandResult cr;
    cr.label     = "run";
    cr.command   = "python3 cleanup.py";
    cr.ran       = true;
    cr.success   = false;
    cr.exit_code = 1;
    cr.stderr_tail =
        "Traceback (most recent call last):\n"
        "  File \"/tmp/frans_pytest/cleanup.py\", line 9, in <module>\n"
        "    wipe(\"/tmp/scratch\")\n"
        "  File \"/tmp/frans_pytest/cleanup.py\", line 6, in wipe\n"
        "    shutil.rmtree(path)\n"
        "  File \"/usr/lib/python3.12/shutil.py\", line 775, in rmtree\n"
        "    onexc(os.lstat, path, err)\n"
        "  File \"/usr/lib/python3.12/shutil.py\", line 773, in rmtree\n"
        "    orig_st = os.lstat(path, dir_fd=dir_fd)\n"
        "FileNotFoundError: [Errno 2] No such file or directory: "
        "'/tmp/scratch'\n";
    pr.commands.push_back(std::move(cr));

    ProjectInfo info;
    info.project_root     = "/tmp/frans_pytest";
    info.sandbox_root     = "/tmp/frans_pytest";
    info.kind             = BuildSystemKind::Standalone;
    info.primary_language = "Python";

    StandaloneBuildSystem sys;
    sys.parseOutput(pr, info);

    bool ok = true;

    if (pr.related_files.empty()) {
        std::printf("  [FAIL] related_files kosong\n");
        ok = false;
    } else {
        const auto& rf = pr.related_files.front();
        std::printf("  frame dipilih: %s:%d\n", rf.path.c_str(), rf.line);

        if (rf.path != "cleanup.py") {
            std::printf("  [FAIL] expected path 'cleanup.py', got '%s'\n",
                        rf.path.c_str());
            ok = false;
        }
        if (rf.line != 9) {
            std::printf("  [FAIL] expected line 9 (frame pertama user), "
                        "got %d\n", rf.line);
            ok = false;
        }
    }

    if (pr.error_message.find("FileNotFoundError") == std::string::npos) {
        std::printf("  [FAIL] error_message tidak mengandung "
                    "'FileNotFoundError': '%s'\n",
                    pr.error_message.c_str());
        ok = false;
    } else {
        std::printf("  error_message: %s\n", pr.error_message.c_str());
    }

    std::printf("  => %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

void listScenarios(const std::vector<Scenario>& scenarios) {
    std::printf("Scenario tersedia:\n");
    for (const auto& s : scenarios) {
        std::printf("  %-24s %s\n", s.name.c_str(), s.description.c_str());
    }
    std::printf("  %-24s Cek SandboxedFileSystem::isPathSafe()\n",
                "sandbox-path-safety");
    std::printf("  %-24s Cek parseOutput pilih frame Python user\n",
                "traceback-frame-picking");
}

void dumpScenario(const Scenario& s) {
    const std::string input = expandFences(s.raw_input);
    std::fwrite(input.data(), 1, input.size(), stdout);
    if (input.empty() || input.back() != '\n') std::putchar('\n');
}

} // namespace

int main(int argc, char** argv) {
    const auto scenarios = buildScenarios();

    if (argc >= 2 && std::strcmp(argv[1], "--list") == 0) {
        listScenarios(scenarios);
        return 0;
    }

    if (argc >= 3 && std::strcmp(argv[1], "--dump") == 0) {
        for (const auto& s : scenarios) {
            if (s.name == argv[2]) {
                dumpScenario(s);
                return 0;
            }
        }
        std::fprintf(stderr, "scenario '%s' tidak ada\n", argv[2]);
        return 2;
    }

    int pass = 0;
    int fail = 0;

    for (const auto& s : scenarios) {
        if (runScenario(s)) ++pass;
        else                ++fail;
    }

    if (runSandboxTest())          ++pass;
    else                            ++fail;

    if (runTracebackPickingTest()) ++pass;
    else                            ++fail;

    std::printf("\n=== Summary: %d passed, %d failed, %d total ===\n",
                pass, fail, pass + fail);
    return fail == 0 ? 0 : 1;
}