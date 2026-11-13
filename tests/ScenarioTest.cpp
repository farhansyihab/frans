// tests/ScenarioTest.cpp
//
// Runner test scenario untuk Manual Import + Danger Check + Sandbox
// + error parser (traceback frame picking) + PromptGenerator (Fase A #42)
// + FileCollector (Fase B #46) + MarkdownRenderer (Fase B #48).
//
// Mode:
//   frans_scenarios              -> jalankan semua scenario, print PASS/FAIL
//   frans_scenarios --list       -> daftar nama scenario
//   frans_scenarios --dump NAME  -> print isi scenario (paste ke GUI)
//
// Scenario yang di-cover (sesuai Blueprint §9.2 + §9.3 + Fase B #46/#48):
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
//     prompt-generator            -> #42 (Fase A)
//     prompt-format-rules         -> verifikasi aturan format output
//     file-collector-basic        -> #46 (Fase B)
//     markdown-renderer           -> #48 (Fase B)
#include "application/import/MultiFileParser.h"
#include "application/safety/DangerCheckHandler.h"
#include "application/build_runner/BuildReport.h"
#include "application/build_runner/ProjectInfo.h"
#include "application/build_runner/BuildSystems/StandaloneBuildSystem.h"
#include "application/prompt/PromptGenerator.h"
#include "application/ingest/FileCollector.h"
#include "application/ingest/MarkdownRenderer.h"
#include "infrastructure/fs/SandboxedFileSystem.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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
using frans::application::PromptInput;
using frans::application::renderPrompt;
using frans::application::defaultPromptTemplate;
using frans::application::CollectedFile;
using frans::application::CollectOptions;
using frans::application::CollectResult;
using frans::application::collectFiles;
using frans::application::isSecretBasename;
using frans::application::isExcludedPath;
using frans::application::renderTree;
using frans::application::renderFileBlock;
using frans::application::renderMarkdown;
using frans::application::RenderInput;
using frans::application::RenderOptions;
using frans::application::detectLanguageFromExtension;
using frans::application::formatBytesHuman;
using frans::domain::GeneratedFile;

// Hindari literal triple-backtick di source (bisa mengganggu tooling).
// Placeholder "###F###" di-expand menjadi tiga backtick oleh expandFences().
const std::string kFence            = "`" "`" "`";
const std::string kFencePlaceholder = "###F###";

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
        s.file_expects.push_back({"src/main.cpp",   DangerExpect::Safe});
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
        s.file_expects.push_back({"package.json", DangerExpect::Safe});
        s.file_expects.push_back({"src/index.js", DangerExpect::Safe});
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
    if (ext == "js" || ext == "jsx" || ext == "mjs" || ext == "cjs") return "javascript";
    if (ext == "vue") return "vue";
    return {};
}

bool runScenario(const Scenario& s) {
    std::printf("\n=== %s ===\n%s\n",
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

// ---- Test PromptGenerator (Fase A #42) ----
bool runPromptGeneratorTest() {
    std::printf("\n=== prompt-generator ===\n");
    std::printf("  Cek PromptGenerator::renderPrompt — placeholder, "
                "blok kondisional, template kosong, placeholder tak "
                "dikenal, blok tanpa penutup, tail 8 KB.\n");

    int fails = 0;
    auto check = [&](const char* name, const std::string& actual,
                     const std::string& expected_substr, bool should_contain) {
        const bool has = actual.find(expected_substr) != std::string::npos;
        const bool pass = (has == should_contain);
        if (!pass) ++fails;
        std::printf("  %s %-40s %s '%s'\n",
                    pass ? "[ok]  " : "[FAIL]",
                    name,
                    should_contain ? "contains" : "NOT contains",
                    expected_substr.c_str());
        if (!pass) {
            std::printf("        actual: '%s'\n", actual.c_str());
        }
    };

    // ---- Test 1: semua blok aktif ----
    {
        PromptInput in;
        in.tmpl = "REQ={{REQUEST}} CTX={{CONTEXT_FILE_NAME}} "
                  "{{#ERROR}}ERR={{ERROR_OUTPUT}}{{/ERROR}} "
                  "{{#CONTEXT}}HAS_CTX{{/CONTEXT}} "
                  "{{#NEWFILES}}NEW={{NEW_FILES_HINT}}{{/NEWFILES}}";
        in.request           = "buat fungsi foo";
        in.error_output      = "Traceback...";
        in.context_file_name = "SourceCode.md";
        in.new_files_hint    = "src/new.py";

        const std::string out = renderPrompt(in);
        check("all-blocks: request",     out, "REQ=buat fungsi foo", true);
        check("all-blocks: ctx name",    out, "CTX=SourceCode.md",   true);
        check("all-blocks: error on",    out, "ERR=Traceback...",    true);
        check("all-blocks: context on",  out, "HAS_CTX",             true);
        check("all-blocks: newfiles on", out, "NEW=src/new.py",      true);
    }

    // ---- Test 2: tanpa error_output -> blok ERROR tidak dirender ----
    {
        PromptInput in;
        in.tmpl = "REQ={{REQUEST}} {{#ERROR}}ERR={{ERROR_OUTPUT}}{{/ERROR}} END";
        in.request      = "foo";
        in.error_output = "";

        const std::string out = renderPrompt(in);
        check("no-error: request", out, "REQ=foo", true);
        check("no-error: no ERR",  out, "ERR=",    false);
        check("no-error: END",     out, "END",     true);
    }

    // ---- Test 3: tanpa context_file_name -> blok CONTEXT tidak dirender ----
    {
        PromptInput in;
        in.tmpl = "{{#CONTEXT}}HAS_CTX{{/CONTEXT}} DONE";
        in.context_file_name = "";

        const std::string out = renderPrompt(in);
        check("no-ctx: no HAS_CTX", out, "HAS_CTX", false);
        check("no-ctx: DONE",       out, "DONE",    true);
    }

    // ---- Test 4: tanpa new_files_hint -> blok NEWFILES tidak dirender ----
    {
        PromptInput in;
        in.tmpl = "{{#NEWFILES}}NEW={{NEW_FILES_HINT}}{{/NEWFILES}} OK";
        in.new_files_hint = "";

        const std::string out = renderPrompt(in);
        check("no-newfiles: no NEW", out, "NEW=", false);
        check("no-newfiles: OK",     out, "OK",   true);
    }

    // ---- Test 5: placeholder tak dikenal -> dibiarkan apa adanya ----
    {
        PromptInput in;
        in.tmpl = "A={{REQUEST}} B={{UNKNOWN}} C={{CONTEXT_FILE_NAME}}";
        in.request           = "req";
        in.context_file_name = "ctx";

        const std::string out = renderPrompt(in);
        check("unknown-ph: A=req",  out, "A=req",         true);
        check("unknown-ph: B kept", out, "B={{UNKNOWN}}", true);
        check("unknown-ph: C=ctx",  out, "C=ctx",         true);
    }

    // ---- Test 6: template kosong -> pakai default ----
    {
        PromptInput in;
        in.tmpl              = "";
        in.request           = "test request";
        in.context_file_name = "SourceCode.md";

        const std::string out = renderPrompt(in);
        check("empty-tmpl: has default marker",
              out, "Full file content, not diff", true);
        check("empty-tmpl: has request",
              out, "test request", true);
        check("empty-tmpl: has context block",
              out, "SourceCode.md", true);
        check("empty-tmpl: no error block (error_output kosong)",
              out, "--- error output ---", false);
    }

    // ---- Test 7: blok tanpa penutup -> dirender apa adanya ----
    {
        PromptInput in;
        in.tmpl         = "BEFORE {{#ERROR}}no-closer END";
        in.error_output = "err";

        const std::string out = renderPrompt(in);
        check("no-closer: BEFORE",    out, "BEFORE",        true);
        check("no-closer: tag kept",  out, "{{#ERROR}}",    true);
        check("no-closer: rest kept", out, "no-closer END", true);
    }

    // ---- Test 8: error_output di-tail +/- 8 KB ----
    {
        PromptInput in;
        in.tmpl = "{{ERROR_OUTPUT}}";
        std::string big(20 * 1024, 'x');
        in.error_output = big;

        const std::string out = renderPrompt(in);
        const bool truncated = out.size() < big.size();
        const bool has_prefix = out.find("truncated") != std::string::npos;
        const bool pass = truncated && has_prefix;
        if (!pass) ++fails;
        std::printf("  %s %-40s truncated=%s has_prefix=%s (out=%zu, in=%zu)\n",
                    pass ? "[ok]  " : "[FAIL]",
                    "error-tail-8kb",
                    truncated  ? "true" : "false",
                    has_prefix ? "true" : "false",
                    out.size(), big.size());
    }

    // ---- Test 9: default template valid ----
    {
        const std::string& d = defaultPromptTemplate();
        const bool not_empty     = !d.empty();
        const bool has_request   = d.find("{{REQUEST}}") != std::string::npos;
        const bool has_ctx_block = d.find("{{#CONTEXT}}") != std::string::npos;
        const bool ok = not_empty && has_request && has_ctx_block;
        if (!ok) ++fails;
        std::printf("  %s %-40s not_empty=%d has_REQUEST=%d has_CTX=%d\n",
                    ok ? "[ok]  " : "[FAIL]",
                    "default-template-valid",
                    (int)not_empty, (int)has_request, (int)has_ctx_block);
    }

    // ---- Test 10: placeholder di dalam blok kondisional ikut di-substitute ----
    {
        PromptInput in;
        in.tmpl              = "{{#CONTEXT}}CTX={{CONTEXT_FILE_NAME}}{{/CONTEXT}}";
        in.context_file_name = "SourceCode.md";

        const std::string out = renderPrompt(in);
        check("inner-ph: substituted", out, "CTX=SourceCode.md", true);
    }

    // ---- Test 11: "{{" tanpa "}}" di akhir template ----
    {
        PromptInput in;
        in.tmpl    = "BEFORE {{REQUEST}} DAN {{";
        in.request = "req";

        const std::string out = renderPrompt(in);
        check("unclosed-open: BEFORE", out, "BEFORE req DAN {{", true);
    }

    std::printf("  => %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0;
}

// ---- Test aturan format output (sinkronisasi dengan format user) ----
bool runPromptFormatRulesTest() {
    std::printf("\n=== prompt-format-rules ===\n");
    std::printf("  Verifikasi default template mengandung semua aturan format "
                "output yang disinkronkan.\n");

    const std::string& d = defaultPromptTemplate();
    int fails = 0;
    auto check = [&](const char* name, const std::string& needle) {
        const bool has = d.find(needle) != std::string::npos;
        if (!has) ++fails;
        std::printf("  %s %-50s contains '%s'\n",
                    has ? "[ok]  " : "[FAIL]",
                    name,
                    needle.c_str());
    };

    check("rule-2: File: <path>",          "File: <path>");
    check("rule-2: fenced code block",     "fenced code block");
    check("rule-3: Path relatif",          "Path relatif");
    check("rule-4: awal baris",            "awal baris");
    check("rule-4: tidak ada indentasi",   "tidak ada indentasi");
    check("rule-5: SEBELUM memulai File:", "SEBELUM memulai File:");
    check("rule-6: tiga backtick",         "tiga backtick");
    check("rule-6: merusak parser",        "merusak parser");
    check("rule-7: tidak berubah",         "tidak berubah");
    check("rule-8: file binary",           "file binary");
    check("rule-9: Full file content",     "Full file content");
    check("rule-9: bukan patch",           "bukan patch");

    std::printf("  => %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0;
}

// ---- Test FileCollector (Fase B #46) ----
bool runFileCollectorTest() {
    std::printf("\n=== file-collector-basic ===\n");
    std::printf("  Cek FileCollector::collectFiles — secret, exclude, "
                "binary, size, output-dir.\n");

    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec);
    if (ec) {
        std::printf("  [SKIP] temp_directory_path gagal: %s\n",
                    ec.message().c_str());
        return true;
    }
    tmp /= "frans_test_collector_v1";
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);
    if (ec) {
        std::printf("  [SKIP] create_directories gagal: %s\n",
                    ec.message().c_str());
        return true;
    }

    auto write = [&](const std::string& rel, const std::string& content) {
        fs::path p = tmp / rel;
        std::error_code ec2;
        fs::create_directories(p.parent_path(), ec2);
        std::ofstream f(p, std::ios::binary);
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
    };

    write("hello.py",     "print('hi')\n");
    write("src/main.cpp", "#include <iostream>\nint main(){}\n");
    write("a/b/c.txt",    "hello\n");
    write(".env",                    "SECRET=xxx\n");
    write("config/credentials.json", "{}\n");
    write("secrets.prod.yaml",       "api_key: xxx\n");
    write("node_modules/x.js", "module.exports={};\n");
    write("build/out.o",       "binary-object\n");

    {
        std::ofstream f(tmp / "big.bin", std::ios::binary);
        std::string big(300 * 1024, 'x');
        f.write(big.data(), static_cast<std::streamsize>(big.size()));
    }
    {
        std::ofstream f(tmp / "data.bin", std::ios::binary);
        const char payload[] = {'a', 'b', 'c', '\0', 'd', 'e', 'f'};
        f.write(payload, sizeof(payload));
    }

    const fs::path out_dir = tmp / "out";
    fs::create_directories(out_dir, ec);
    write("out/SourceCode.md", "# harus di-exclude\n");

    CollectOptions opt;
    opt.max_file_bytes  = 256 * 1024;
    opt.exclude_secrets = true;
    opt.output_dir      = out_dir.string();

    CollectResult result = collectFiles(tmp.string(), opt, nullptr);

    if (result.used_git) {
        std::printf("  [SKIP] temp dir terdeteksi sebagai git repo — "
                    "test ini dirancang untuk fallback walker.\n");
        fs::remove_all(tmp, ec);
        return true;
    }

    bool ok = result.ok;
    if (!ok) {
        std::printf("  [FAIL] collectFiles gagal: %s\n", result.error.c_str());
    }

    auto findFile = [&](const std::string& rel) -> const CollectedFile* {
        for (const auto& f : result.files) {
            if (f.relative_path == rel) return &f;
        }
        return nullptr;
    };

    auto checkExist = [&](const char* rel, bool should_exist) {
        const CollectedFile* f = findFile(rel);
        const bool found = (f != nullptr);
        const bool pass = (found == should_exist);
        if (!pass) ok = false;
        std::printf("  %s exist %-32s found=%d expected=%d\n",
                    pass ? "[ok]  " : "[FAIL]",
                    rel, found ? 1 : 0, should_exist ? 1 : 0);
    };

    auto checkSkip = [&](const char* rel, const char* expected_reason) {
        const CollectedFile* f = findFile(rel);
        if (!f) {
            ok = false;
            std::printf("  [FAIL] skip  %-32s NOT FOUND\n", rel);
            return;
        }
        const bool pass = f->skipped && f->skip_reason == expected_reason;
        if (!pass) ok = false;
        std::printf("  %s skip   %-32s reason='%s' expected='%s'\n",
                    pass ? "[ok]  " : "[FAIL]",
                    rel,
                    f->skip_reason.c_str(),
                    expected_reason);
    };

    checkExist("hello.py",     true);
    checkExist("src/main.cpp", true);
    checkExist("a/b/c.txt",    true);
    checkSkip (".env",                   "secret");
    checkSkip ("config/credentials.json", "secret");
    checkSkip ("secrets.prod.yaml",      "secret");
    checkExist("node_modules/x.js", false);
    checkExist("build/out.o",       false);
    checkSkip ("big.bin",  "too_large");
    checkSkip ("data.bin", "binary");
    checkExist("out/SourceCode.md", false);

    auto helperCheck = [&](const char* label, bool cond) {
        if (!cond) ok = false;
        std::printf("  %s helper %-40s\n",
                    cond ? "[ok]  " : "[FAIL]", label);
    };

    helperCheck("isSecretBasename('.env')",
                isSecretBasename(".env"));
    helperCheck("isSecretBasename('id_rsa')",
                isSecretBasename("id_rsa"));
    helperCheck("isSecretBasename('foo.pem')",
                isSecretBasename("foo.pem"));
    helperCheck("isSecretBasename('secrets.prod.yaml')",
                isSecretBasename("secrets.prod.yaml"));
    helperCheck("!isSecretBasename('main.cpp')",
                !isSecretBasename("main.cpp"));
    helperCheck("!isSecretBasename('.envrc')",
                !isSecretBasename(".envrc"));
    helperCheck("isExcludedPath('node_modules/x.js', {})",
                isExcludedPath("node_modules/x.js", {}));
    helperCheck("isExcludedPath('build/out.o', {})",
                isExcludedPath("build/out.o", {}));
    helperCheck("isExcludedPath('a/__pycache__/x.pyc', {})",
                isExcludedPath("a/__pycache__/x.pyc", {}));
    helperCheck("!isExcludedPath('src/main.cpp', {})",
                !isExcludedPath("src/main.cpp", {}));
    helperCheck("isExcludedPath('foo.log', {'*.log'})",
                isExcludedPath("foo.log", {"*.log"}));
    helperCheck("!isExcludedPath('foo.txt', {'*.log'})",
                !isExcludedPath("foo.txt", {"*.log"}));

    if (result.files.size() >= 2) {
        bool sorted = true;
        for (std::size_t i = 1; i < result.files.size(); ++i) {
            if (result.files[i - 1].relative_path
                > result.files[i].relative_path)
            {
                sorted = false;
                break;
            }
        }
        helperCheck("files terurut leksikografis", sorted);
    }

    fs::remove_all(tmp, ec);
    std::printf("  => %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ---- Test MarkdownRenderer (Fase B #48) ----
//
// Cek:
//   - renderTree: struktur tree, penanda "*", sorting alphabetical,
//     intermediate dirs dibuat otomatis.
//   - renderFileBlock: separator + FILE: path + separator + konten.
//   - renderMarkdown: header + tree + file blocks lengkap.
//   - detectLanguageFromExtension: pemetaan ekstensi -> bahasa.
//   - formatBytesHuman: B / KB / MB.
bool runMarkdownRendererTest() {
    std::printf("\n=== markdown-renderer ===\n");
    std::printf("  Cek MarkdownRenderer — tree, file block, render lengkap, "
                "language detection, format bytes.\n");

    int fails = 0;
    auto checkContains = [&](const char* label, const std::string& hay,
                             const std::string& needle) {
        const bool ok = hay.find(needle) != std::string::npos;
        if (!ok) ++fails;
        std::printf("  %s %-44s contains '%s'\n",
                    ok ? "[ok]  " : "[FAIL]", label, needle.c_str());
        if (!ok) std::printf("        actual: '%s'\n", hay.c_str());
    };
    auto checkNotContains = [&](const char* label, const std::string& hay,
                                const std::string& needle) {
        const bool ok = hay.find(needle) == std::string::npos;
        if (!ok) ++fails;
        std::printf("  %s %-44s NOT contains '%s'\n",
                    ok ? "[ok]  " : "[FAIL]", label, needle.c_str());
    };

    // ---- Test 1: renderTree dasar ----
    {
        std::vector<CollectedFile> files = {
            {"README.md",       100, true, false, ""},
            {"src/main.cpp",    200, true, false, ""},
            {"src/util.h",       50, true, false, ""},
            {"tests/test.cpp",  150, true, false, ""},
        };
        std::vector<std::string> starred = {"src/main.cpp", "README.md"};

        const std::string tree = renderTree(files, "demo-repo", starred);

        checkContains("tree: header",        tree, "Directory structure:");
        checkContains("tree: root",          tree, "└── demo-repo/");
        checkContains("tree: README starred", tree, "README.md *");
        checkContains("tree: src dir",       tree, "src/");
        checkContains("tree: main.cpp starred", tree, "main.cpp *");
        checkContains("tree: util.h no star", tree, "util.h");
        checkNotContains("tree: util.h no *", tree, "util.h *");
        checkContains("tree: tests dir",     tree, "tests/");
        checkContains("tree: test.cpp",      tree, "test.cpp");
        // util.h TIDAK boleh berbintang
        checkNotContains("tree: util.h clean", tree, "util.h *");
    }

    // ---- Test 2: sorting alphabetical (file & dir bercampur) ----
    {
        std::vector<CollectedFile> files = {
            {"z.txt",      10, true, false, ""},
            {"a.txt",      10, true, false, ""},
            {"m/b.txt",    10, true, false, ""},
            {"m/a.txt",    10, true, false, ""},
        };
        const std::string tree = renderTree(files, "repo", {});

        // Urutan yang diharapkan:
        //   a.txt
        //   m/
        //     a.txt
        //     b.txt
        //   z.txt
        const auto pos_a = tree.find("a.txt");
        const auto pos_m = tree.find("m/");
        const auto pos_z = tree.find("z.txt");
        const bool order_ok = (pos_a < pos_m) && (pos_m < pos_z);
        if (!order_ok) ++fails;
        std::printf("  %s %-44s alphabetical order\n",
                    order_ok ? "[ok]  " : "[FAIL]", "tree-sort");
    }

    // ---- Test 3: intermediate dirs dibuat otomatis ----
    {
        std::vector<CollectedFile> files = {
            {"a/b/c/d.txt", 10, true, false, ""},
        };
        const std::string tree = renderTree(files, "repo", {});

        checkContains("tree: a/",  tree, "a/");
        checkContains("tree: b/",  tree, "b/");
        checkContains("tree: c/",  tree, "c/");
        checkContains("tree: d.txt", tree, "d.txt");
    }

    // ---- Test 4: file yang di-skip tidak muncul di tree ----
    {
        std::vector<CollectedFile> files = {
            {"visible.txt", 10, true, false, ""},
            {"secret.env",  10, true, true, "secret"},
        };
        const std::string tree = renderTree(files, "repo", {});

        checkContains("tree: visible ada",  tree, "visible.txt");
        checkNotContains("tree: secret skip", tree, "secret.env");
    }

    // ---- Test 5: renderFileBlock ----
    {
        const std::string block =
            renderFileBlock("src/main.cpp", "#include <iostream>\nint main() {}\n");

        checkContains("block: separator atas", block,
                      "================================================");
        checkContains("block: FILE: path",     block, "FILE: src/main.cpp");
        checkContains("block: konten",         block, "#include <iostream>");
        checkContains("block: akhir newline",  block, "int main() {}\n");
    }

    // ---- Test 6: renderFileBlock tanpa trailing newline ----
    {
        const std::string block =
            renderFileBlock("a.txt", "no newline at end");

        checkContains("block: auto-newline", block, "no newline at end\n");
    }

    // ---- Test 7: renderMarkdown lengkap ----
    {
        RenderInput in;
        in.all_files = {
            {"README.md",    100, true, false, ""},
            {"src/main.cpp", 200, true, false, ""},
            {"src/util.h",    50, true, false, ""},
        };
        in.starred_paths = {"src/main.cpp", "README.md"};
        in.file_contents = {
            {"README.md",    "# Demo\n"},
            {"src/main.cpp", "#include <iostream>\n"},
        };
        in.options.repo_name            = "demo-repo";
        in.options.header_title         = "SourceCode.md";
        in.options.created_at           = "2026-11-13 14:05";
        in.options.request_summary      = "buat fitur X";
        in.options.total_files_included = 2;
        in.options.total_content_bytes  = 38 * 1024 + 200;  // 38.2 KB

        const std::string out = renderMarkdown(in);

        checkContains("md: header title",  out, "# SourceCode.md: demo-repo");
        checkContains("md: timestamp",     out, "2026-11-13 14:05");
        checkContains("md: file count",    out, "2 file disertakan");
        checkContains("md: size",          out, "38.2 KB");
        checkContains("md: permintaan",    out, "Permintaan: buat fitur X");
        checkContains("md: marker legend", out,
                      "Tanda \"*\" pada struktur di bawah");
        checkContains("md: tree root",     out, "└── demo-repo/");
        checkContains("md: README starred", out, "README.md *");
        checkContains("md: util no star",  out, "util.h");
        checkNotContains("md: util clean", out, "util.h *");
        checkContains("md: block README",  out, "FILE: README.md");
        checkContains("md: block main",    out, "FILE: src/main.cpp");
        checkNotContains("md: util no block", out, "FILE: src/util.h");
        checkContains("md: konten README", out, "# Demo");
        checkContains("md: konten main",   out, "#include <iostream>");
    }

    // ---- Test 8: renderMarkdown tanpa permintaan ----
    {
        RenderInput in;
        in.all_files = {{"a.txt", 10, true, false, ""}};
        in.starred_paths = {"a.txt"};
        in.file_contents = {{"a.txt", "hello\n"}};
        in.options.repo_name            = "r";
        in.options.header_title         = "Ingest";
        in.options.total_files_included = 1;
        in.options.total_content_bytes  = 6;

        const std::string out = renderMarkdown(in);
        checkNotContains("md: no permintaan line", out, "Permintaan:");
        checkContains("md: header tetap ada", out, "# Ingest: r");
    }

    // ---- Test 9: starred path tidak ada di file_contents -> warning + skip ----
    {
        RenderInput in;
        in.all_files = {{"a.txt", 10, true, false, ""}};
        in.starred_paths = {"a.txt", "missing.txt"};
        in.file_contents = {{"a.txt", "hello\n"}};
        in.options.repo_name = "r";

        const std::string out = renderMarkdown(in);
        checkContains("md: a.txt block ada", out, "FILE: a.txt");
        checkNotContains("md: missing.txt skip", out, "FILE: missing.txt");
    }

    // ---- Test 10: detectLanguageFromExtension ----
    {
        auto checkLang = [&](const char* path, const char* expected) {
            const std::string actual = detectLanguageFromExtension(path);
            const bool ok = (actual == expected);
            if (!ok) ++fails;
            std::printf("  %s lang(%-24s) = '%s' (expected '%s')\n",
                        ok ? "[ok]  " : "[FAIL]",
                        path, actual.c_str(), expected);
        };
        checkLang("main.cpp",          "cpp");
        checkLang("header.hpp",        "cpp");
        checkLang("script.py",         "python");
        checkLang("app.js",            "javascript");
        checkLang("app.ts",            "typescript");
        checkLang("data.json",         "json");
        checkLang("config.yaml",       "yaml");
        checkLang("CMakeLists.txt",    "cmake");
        checkLang("Makefile",          "makefile");
        checkLang("Dockerfile",        "dockerfile");
        checkLang("readme.md",         "markdown");
        checkLang("noext",             "");
        checkLang("unknown.xyz",       "");
    }

    // ---- Test 11: formatBytesHuman ----
    {
        auto checkFmt = [&](std::size_t bytes, const char* expected) {
            const std::string actual = formatBytesHuman(bytes);
            const bool ok = (actual == expected);
            if (!ok) ++fails;
            std::printf("  %s fmt(%zu) = '%s' (expected '%s')\n",
                        ok ? "[ok]  " : "[FAIL]",
                        bytes, actual.c_str(), expected);
        };
        checkFmt(0,          "0 B");
        checkFmt(512,        "512 B");
        checkFmt(1024,       "1.0 KB");
        checkFmt(1536,       "1.5 KB");
        checkFmt(38 * 1024 + 200, "38.2 KB");
        checkFmt(1024 * 1024, "1.0 MB");
        checkFmt(5 * 1024 * 1024 + 123 * 1024, "5.1 MB");
    }

    // ---- Test 12: tree kosong (tidak ada file) ----
    {
        std::vector<CollectedFile> files;
        const std::string tree = renderTree(files, "empty-repo", {});
        checkContains("tree-empty: header", tree, "Directory structure:");
        checkContains("tree-empty: root",   tree, "└── empty-repo/");
        // Tidak ada anak
        checkNotContains("tree-empty: no children", tree, "├──");
        checkNotContains("tree-empty: no children2", tree, "└── ");
    }

    std::printf("  => %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0;
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
    std::printf("  %-24s Cek PromptGenerator::renderPrompt (Fase A #42)\n",
                "prompt-generator");
    std::printf("  %-24s Verifikasi aturan format output\n",
                "prompt-format-rules");
    std::printf("  %-24s Cek FileCollector (Fase B #46)\n",
                "file-collector-basic");
    std::printf("  %-24s Cek MarkdownRenderer (Fase B #48)\n",
                "markdown-renderer");
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
    if (runSandboxTest())           ++pass;
    else                            ++fail;
    if (runTracebackPickingTest())  ++pass;
    else                            ++fail;
    if (runPromptGeneratorTest())   ++pass;
    else                            ++fail;
    if (runPromptFormatRulesTest()) ++pass;
    else                            ++fail;
    if (runFileCollectorTest())     ++pass;
    else                            ++fail;
    if (runMarkdownRendererTest())  ++pass;
    else                            ++fail;

    std::printf("\n=== Summary: %d passed, %d failed, %d total ===\n",
                pass, fail, pass + fail);
    return fail == 0 ? 0 : 1;
}
