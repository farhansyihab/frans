// ~/AgenAI/frans/tests/multilang_smoke_test.cpp
//
// Stage 10d — multi-language integration smoke test.
//
// Menguji:
//   1.  TreeSitterGraphBuilder::build() men-discover 4 file source
//       dari fixture mixed (README.md di-skip).
//   2.  findBySymbol("Service") → ≥ 3 (Python, C++, TS).
//   3.  findBySymbol("Service::run") → ≥ 2 (C++ header + impl — dual-file).
//   4.  findBySymbol("Service.run") → ≥ 1 (TS; Python pakai plain "run"
//       dengan parent_class="Service", C++ pakai "::").
//   5.  findBySymbol("run") → ≥ 1 (Python method — plain naming).
//   6.  ProjectGraphCache: getOrBuild() → file_count == 4.
//   7.  Rebuild triggered saat file .ts dimodifikasi (mtime check).
//   8.  Rebuild triggered saat file baru ditambahkan.
//   9.  Non-code file (README.md) tidak muncul di graph.allFilePaths().
//   10. Determinism — dua kali build() dengan state sama → graph identical.
//   11. ContextSelector cross-language: task "refactor Service class"
//       → snippet dari ≥ 2 bahasa berbeda.
//   12. supportedExtensions() mencakup semua bahasa yang di-build.
//
// Catatan: assertion #4 sedikit berbeda dari blueprint §6.5 tabel #3
// yang menyebut "minimal 2 (dari C++ dan TS — Python pakai Service.run
// juga karena method)". Klaim tentang Python itu tidak akurat — Stage 5
// PythonAstVisitor menyimpan method name sebagai plain `run` dengan
// parent_class terpisah. Kita verifikasi perilaku AKTUAL (≥ 1), plus
// assertion dual-file C++ via `Service::run` (≥ 2).

#include "application/context_budget/ContextBudgetManager.h"
#include "application/context_selector/ContextSelector.h"
#include "application/project/ProjectGraphCache.h"
#include "code_intelligence/builder/TreeSitterGraphBuilder.h"
#include "code_intelligence/graph/DependencyGraph.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>     // Stage 10d fix: untuk std::this_thread::sleep_for
#include <vector>

using namespace frans::code_intelligence;
using frans::application::ProjectGraphCache;

namespace fs = std::filesystem;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const std::string& label) {
    if (cond) {
        ++g_pass;
        std::printf("  PASS  %s\n", label.c_str());
    } else {
        ++g_fail;
        std::printf("  FAIL  %s\n", label.c_str());
    }
}

void section(const char* name) {
    std::printf("\n[%s]\n", name);
}

// Ekstrak extension (lowercase) dari file path.
std::string extOf(const std::string& path) {
    return fs::path(path).extension().string();
}

// Ambil set extension unik dari hasil ContextSelection.snippets.
std::set<std::string> uniqueExtensions(const std::vector<CodeSnippet>& snips) {
    std::set<std::string> out;
    for (const auto& s : snips) {
        out.insert(extOf(s.file_path));
    }
    return out;
}

// Tulis file (helper untuk setup temp dir).
void writeFile(const fs::path& p, const std::string& content) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << content;
}

// Suffix unik untuk temp dir.
std::string uniqueSuffix() {
    const auto now = std::chrono::steady_clock::now()
                         .time_since_epoch()
                         .count();
    return std::to_string(now);
}

} // namespace

int main() {
    const std::string fixture_dir = FIXTURE_DIR;

    std::printf("=== frans multilang smoke test (Stage 10d) ===\n");
    std::printf("fixture dir: %s\n", fixture_dir.c_str());

    // =====================================================================
    // 1. Direct build — discovery + parse
    // =====================================================================
    section("TreeSitterGraphBuilder — mixed fixture");
    DependencyGraph graph_snapshot;   // copy untuk assertion berikutnya
    {
        TreeSitterGraphBuilder builder;
        builder.build(fixture_dir);
        const auto& g = builder.graph();

        // --- fileCount: 4 (.py, .hpp, .cpp, .ts); README.md di-skip ---
        check(g.fileCount() == 4,
              "fileCount == 4 (py + hpp + cpp + ts; README.md di-skip)");

        // --- non-code file tidak masuk graph ---
        bool readme_present = false;
        for (const auto& p : g.allFilePaths()) {
            if (p.find("README.md") != std::string::npos) {
                readme_present = true;
                break;
            }
        }
        check(!readme_present, "README.md TIDAK ada di allFilePaths()");

        // --- symbol collision cross-language ---
        const auto svc = g.findBySymbol("Service");
        check(svc.size() >= 3,
              "findBySymbol(\"Service\") >= 3 (py + hpp + ts)");
        {
            std::set<std::string> exts;
            for (const auto& s : svc) exts.insert(extOf(s.file_path));
            check(exts.count(".py") == 1
               && exts.count(".hpp") == 1
               && exts.count(".ts") == 1,
                  "  ketiga bahasa berbeda terwakili (.py, .hpp, .ts)");
        }

        // --- C++ dual-file merge: Service::run dari header + impl ---
        const auto cpp_run = g.findBySymbol("Service::run");
        check(cpp_run.size() >= 2,
              "findBySymbol(\"Service::run\") >= 2 (hpp + cpp)");
        {
            std::set<std::string> exts;
            for (const auto& s : cpp_run) exts.insert(extOf(s.file_path));
            check(exts.count(".hpp") == 1 && exts.count(".cpp") == 1,
                  "  C++ dual-file: header + impl terwakili");
        }

        // --- TS qualified name pakai dot ---
        const auto ts_run = g.findBySymbol("Service.run");
        check(ts_run.size() >= 1,
              "findBySymbol(\"Service.run\") >= 1 (TS convention: dot)");

        // --- Python plain method name ---
        const auto py_run = g.findBySymbol("run");
        check(py_run.size() >= 1,
              "findBySymbol(\"run\") >= 1 (Python: plain name + parent_class)");

        // Snapshot graph untuk determinism check nanti.
        graph_snapshot = g;
    }

    // =====================================================================
    // 2. Determinism — build 2x → graph identical
    // =====================================================================
    section("Determinism");
    {
        TreeSitterGraphBuilder b1;
        TreeSitterGraphBuilder b2;
        b1.build(fixture_dir);
        b2.build(fixture_dir);

        check(b1.graph().fileCount()   == b2.graph().fileCount(),
              "fileCount identik");
        check(b1.graph().symbolCount() == b2.graph().symbolCount(),
              "symbolCount identik");
        check(b1.graph().edgeCount()   == b2.graph().edgeCount(),
              "edgeCount identik");

        const auto files1 = b1.graph().allFilePaths();
        const auto files2 = b2.graph().allFilePaths();
        check(files1 == files2, "file list identik (byte-for-byte)");

        // Symbol query determinism
        const auto s1 = b1.graph().findBySymbol("Service");
        const auto s2 = b2.graph().findBySymbol("Service");
        bool same = (s1.size() == s2.size());
        if (same) {
            for (std::size_t i = 0; i < s1.size(); ++i) {
                if (s1[i].file_path  != s2[i].file_path
                 || s1[i].start_line != s2[i].start_line
                 || s1[i].symbol_name != s2[i].symbol_name) {
                    same = false;
                    break;
                }
            }
        }
        check(same, "findBySymbol(\"Service\") urutan identik");
    }

    // =====================================================================
    // 3. ContextSelector cross-language
    // =====================================================================
    section("ContextSelector — cross-language snippet selection");
    {
        frans::application::ContextBudgetManager budget_mgr;
        const int code_budget =
            budget_mgr.allocate(4096).code_context_tokens;

        frans::application::ContextSelector selector;
        const auto sel = selector.selectContext(
            "refactor Service class", graph_snapshot, code_budget);

        const auto exts = uniqueExtensions(sel.snippets);
        check(exts.size() >= 2,
              "selectContext() -> snippet dari >= 2 bahasa berbeda");

        // Print ext yang terpilih (diagnostik).
        std::string joined;
        for (const auto& e : exts) {
            if (!joined.empty()) joined += ", ";
            joined += e;
        }
        std::printf("        (extensions terpilih: %s)\n", joined.c_str());
    }

    // =====================================================================
    // 4. ProjectGraphCache — getOrBuild + file_count
    // =====================================================================
    section("ProjectGraphCache — getOrBuild + status");
    {
        ProjectGraphCache cache;
        const auto& g = cache.getOrBuild(fixture_dir);

        check(g.fileCount() == 4,
              "graph.fileCount() == 4 (via cache)");

        const auto st = cache.status();
        check(st.valid,           "status.valid == true");
        check(st.file_count == 4, "status.file_count == 4");
        check(st.build_count == 1, "status.build_count == 1 (fresh)");
    }

    // =====================================================================
    // 5. Rebuild trigger — file baru ditambah → build_count naik
    // =====================================================================
    section("Rebuild trigger — file baru ditambah");
    {
        // Isolasi dengan temp dir supaya tidak mengotori fixture.
        const fs::path tmp =
            fs::temp_directory_path() / ("frans_ml_test_" + uniqueSuffix());
        fs::create_directories(tmp);

        writeFile(tmp / "a.py",   "class A:\n    def foo(self): pass\n");
        writeFile(tmp / "b.ts",   "export class B { bar() {} }\n");

        ProjectGraphCache cache;
        (void)cache.getOrBuild(tmp.string());
        const int build1 = cache.status().build_count;

        // Tambah file .cpp baru.
        writeFile(tmp / "c.cpp",
                  "#include <cstdio>\nvoid f() { std::printf(\"x\"); }\n");

        // Panggilan berikutnya harus trigger rebuild karena file baru.
        const auto& g2 = cache.getOrBuild(tmp.string());
        const int build2 = cache.status().build_count;

        check(build2 == build1 + 1,
              "build_count +1 setelah file baru ditambahkan");
        check(g2.fileCount() == 3,
              "fileCount == 3 setelah penambahan");

        // Cleanup
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }

    // =====================================================================
    // 6. Rebuild trigger — file dimodifikasi → build_count naik
    // =====================================================================
    section("Rebuild trigger — file dimodifikasi (mtime)");
    {
        const fs::path tmp =
            fs::temp_directory_path() / ("frans_ml_test_" + uniqueSuffix());
        fs::create_directories(tmp);

        writeFile(tmp / "x.ts",
                  "export function alpha() { return 1; }\n");

        ProjectGraphCache cache;
        (void)cache.getOrBuild(tmp.string());
        const int build1 = cache.status().build_count;

        // Beri delay kecil supaya mtime pasti berbeda (beberapa FS
        // resolusi mtime ~1 detik).
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        writeFile(tmp / "x.ts",
                  "export function alpha() { return 2; }\n"
                  "export function beta()  { return 3; }\n");

        (void)cache.getOrBuild(tmp.string());
        const int build2 = cache.status().build_count;

        check(build2 == build1 + 1,
              "build_count +1 setelah file dimodifikasi");

        std::error_code ec;
        fs::remove_all(tmp, ec);
    }

    // =====================================================================
    // 7. supportedExtensions() — semua bahasa terwakili
    // =====================================================================
    section("TreeSitterGraphBuilder::supportedExtensions");
    {
        const auto exts = TreeSitterGraphBuilder::supportedExtensions();
        std::set<std::string> s(exts.begin(), exts.end());

        check(s.count(".py") == 1,   ".py terdaftar");
        check(s.count(".cpp") == 1,  ".cpp terdaftar");
        check(s.count(".hpp") == 1,  ".hpp terdaftar");
        check(s.count(".ts") == 1,   ".ts terdaftar");
        check(s.count(".tsx") == 1,  ".tsx terdaftar");
        check(s.count(".js") == 1,   ".js terdaftar");
        check(s.count(".vue") == 1,  ".vue terdaftar");
        check(exts.size() == s.size(),
              "tidak ada duplikat di supportedExtensions()");
    }

    // =====================================================================
    // Summary
    // =====================================================================
    std::printf("\n=== total: %d pass, %d fail ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
