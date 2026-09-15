// ~/AgenAI/frans/tests/manual_import_e2e_test.cpp
//
// Stage 11c — end-to-end smoke test Manual Import pipeline:
//   paste → ManualImporter::preview() → commit() → ProjectGraphCache
//   invalidate → getOrBuild() rebuild → graph berisi file baru.
//
// Test ini adalah INTEGRATION test (bukan unit test). Yang diuji bukan
// hanya ManualImporter sendiri, tapi KONTRAK ANTAR-MODUL:
//
//   1. ManualImporter::preview() mendeteksi multi-file paste dengan benar
//      (3 file, mode multi_file, semua NEW).
//   2. ManualImporter::commit() menulis semua file ke SandboxedFileSystem.
//   3. Setelah commit sukses (written_count > 0), InvalidateFn dipanggil
//      TEPAT SEKALI.
//   4. InvalidateFn → ProjectGraphCache::invalidate() → status.valid=false.
//   5. getOrBuild() berikutnya → REBUILD (bukan cache hit) — build_count
//      bertambah, file_count mencerminkan file baru, symbol_count > 0.
//
// Bonus regresi (mengunci perilaku yang sudah diverifikasi manual):
//   6. Marker "File:" dengan leading whitespace tetap terdeteksi (11b-fix).
//   7. Single-file tanpa marker + tanpa fallback → parse_failed (bukan crash).
//   8. Single-file tanpa marker + fallback → 1 file tertulis dengan nama
//      fallback.
//   9. Semua file Blocked → commit menulis 0 file, InvalidateFn TIDAK
//      dipanggil (karena tidak ada perubahan filesystem).
//
// Butuh tree-sitter (via frans_project → frans_context → frans_code_intel).
// Di CMakeLists, target ini di-guard dengan FRANS_CODE_INTEL_ENABLED.
//
// Jalankan: ./frans_manual_import_e2e_test [sandbox_root]
// Default sandbox_root = /tmp/frans_manual_import_e2e_<pid>

#include "application/manual_import/ManualImporter.h"
#include "application/project/ProjectGraphCache.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>

using frans::application::ImportFilePreview;
using frans::application::ImportResult;
using frans::application::ManualImporter;
using frans::application::ProjectGraphCache;
using frans::infrastructure::SandboxedFileSystem;

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& label) {
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", label.c_str());
    if (!cond) ++g_failures;
}

const ImportFilePreview* findFile(const ImportResult& r,
                                   const std::string& path)
{
    for (const auto& f : r.files) {
        if (f.relative_path == path) return &f;
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    // ---- Resolve sandbox root ----
    std::string root;
    if (argc > 1) {
        root = argv[1];
    } else {
        root = (fs::temp_directory_path()
                / ("frans_manual_import_e2e_" + std::to_string(::getpid())))
                .string();
    }

    std::printf("=== frans_ai manual import E2E test (Stage 11c) ===\n");
    std::printf("root: %s\n\n", root.c_str());

    std::error_code ec;
    fs::remove_all(root, ec);

    SandboxedFileSystem sb(root);
    std::printf("canonical root: %s\n\n", sb.root().string().c_str());
    const std::string canonical_root = sb.root().string();

    // =====================================================================
    // Fixture: tiga file Python valid supaya tree-sitter benar-benar
    // menghasilkan symbol (bukan 0). Kalau parse gagal, symbol_count = 0
    // dan kita tidak bisa membedakan "parser jalan" vs "parser di-skip".
    // =====================================================================
    const std::string multi_paste = R"(File: a.py
```python
def alpha():
    return 1
```

File: b.py
```python
def beta():
    return 2
```

File: c.py
```python
def gamma():
    return 3
```
)";

    // Marker "File:" dengan leading whitespace — regression test 11b-fix.
    const std::string ws_paste = R"(  File: ws_a.py
```python
def wa():
    return 10
```

  File: ws_b.py
```python
def wb():
    return 20
```
)";

    // Single-file tanpa marker — butuh fallback_path.
    const std::string single_no_marker = R"(```python
def alone():
    return 42
```
)";

    // Semua path escape → semua Blocked.
    const std::string blocked_paste = R"(File: ../escape1.py
```python
x = 1
```

File: ../escape2.py
```python
y = 2
```
)";

    // =====================================================================
    // Setup: cache + importer dengan counter invalidate.
    // =====================================================================
    ProjectGraphCache cache;
    int invalidate_calls = 0;

    ManualImporter imp(sb, [&cache, &invalidate_calls]() {
        cache.invalidate();
        ++invalidate_calls;
    });

    // =====================================================================
    // Test 1 — graph baseline: 0 file, build_count = 1
    // =====================================================================
    std::printf("[Test 1] baseline graph pada dir kosong\n");
    {
        (void)cache.getOrBuild(canonical_root);
        const auto st = cache.status();
        check(st.valid,          "valid == true setelah build pertama");
        check(st.build_count == 1, "build_count == 1");
        check(st.file_count == 0, "file_count == 0 (dir kosong)");
    }

    // =====================================================================
    // Test 2 — preview multi-file paste
    // =====================================================================
    std::printf("\n[Test 2] preview multi-file paste (3 file)\n");
    const ImportResult multi_r = imp.preview(multi_paste);
    {
        check(!multi_r.parse_failed,    "parse_failed == false");
        check(multi_r.multi_file,        "multi_file == true");
        check(multi_r.files.size() == 3, "3 file");
        check(multi_r.new_count == 3,    "new_count == 3");
        check(multi_r.blocked_count == 0, "blocked_count == 0");
        check(multi_r.overwrite_count == 0, "overwrite_count == 0");

        check(findFile(multi_r, "a.py") != nullptr, "a.py ada di preview");
        check(findFile(multi_r, "b.py") != nullptr, "b.py ada di preview");
        check(findFile(multi_r, "c.py") != nullptr, "c.py ada di preview");
    }

    // =====================================================================
    // Test 3 — commit menulis file + invalidate cache
    // =====================================================================
    std::printf("\n[Test 3] commit → tulis file + invalidate\n");
    {
        // Pre-commit: cache VALID (belum ada file baru di disk).
        check(cache.status().valid, "pre-commit: cache valid");

        const int calls_before = invalidate_calls;
        const ImportResult commit_r = imp.commit();

        check(commit_r.written_count == 3, "written_count == 3");
        check(commit_r.failed_count  == 0, "failed_count == 0");
        check(commit_r.blocked_count == 0, "blocked_count == 0");

        // KEY ASSERTION: status berubah ke invalid setelah commit.
        check(!cache.status().valid,
              "post-commit: cache.valid == false (invalidate fired)");

        check(invalidate_calls == calls_before + 1,
              "invalidate_calls bertambah tepat 1");

        // Files benar-benar ada di disk (lewat API).
        check(sb.exists("a.py"), "a.py ada di disk");
        check(sb.exists("b.py"), "b.py ada di disk");
        check(sb.exists("c.py"), "c.py ada di disk");

        // Content round-trip.
        check(sb.readFile("a.py").find("def alpha") != std::string::npos,
              "a.py berisi 'def alpha'");
        check(sb.readFile("b.py").find("def beta") != std::string::npos,
              "b.py berisi 'def beta'");
        check(sb.readFile("c.py").find("def gamma") != std::string::npos,
              "c.py berisi 'def gamma'");
    }

    // =====================================================================
    // Test 4 — getOrBuild rebuild, lihat 3 file baru
    // =====================================================================
    std::printf("\n[Test 4] rebuild graph — 3 file, symbols > 0\n");
    {
        const auto& g = cache.getOrBuild(canonical_root);
        const auto st = cache.status();

        check(st.valid, "valid == true setelah rebuild");
        check(st.build_count == 2,
              "build_count == 2 (initial=1, rebuild=2)");
        check(st.file_count == 3, "file_count == 3 (a/b/c.py)");
        check(st.symbol_count > 0,
              "symbol_count > 0 (tree-sitter benar-benar parse)");

        // Akses graph via reference — hanya dipakai dalam scope ini.
        check(g.fileCount() == 3, "graph.fileCount() == 3");
        check(g.symbolCount() > 0, "graph.symbolCount() > 0");
    }

    // =====================================================================
    // Test 5 — marker 'File:' dengan leading whitespace (11b-fix regression)
    // =====================================================================
    std::printf("\n[Test 5] leading whitespace di marker tetap terdeteksi\n");
    {
        const ImportResult ws_r = imp.preview(ws_paste);
        check(!ws_r.parse_failed,
              "parse_failed == false (fix whitespace bekerja)");
        check(ws_r.multi_file,
              "multi_file == true (bukan fallback single-file)");
        check(ws_r.files.size() == 2, "2 file terdeteksi");
        check(findFile(ws_r, "ws_a.py") != nullptr, "ws_a.py ada");
        check(findFile(ws_r, "ws_b.py") != nullptr, "ws_b.py ada");
    }

    // =====================================================================
    // Test 6 — single-file tanpa marker + tanpa fallback → error
    // =====================================================================
    std::printf("\n[Test 6] single-file tanpa fallback → parse gagal\n");
    {
        const ImportResult err_r = imp.preview(single_no_marker, "");
        check(err_r.parse_failed, "parse_failed == true");
        check(!err_r.parse_error.empty(), "parse_error terisi");
        check(err_r.parse_error.find("fallback_path kosong")
                != std::string::npos,
              "pesan menyebut 'fallback_path kosong'");
    }

    // =====================================================================
    // Test 7 — single-file dengan fallback → 1 file, nama fallback
    // =====================================================================
    std::printf("\n[Test 7] single-file + fallback → 1 file\n");
    {
        const ImportResult fb_r = imp.preview(single_no_marker, "alone.py");
        check(!fb_r.parse_failed,      "parse_failed == false");
        check(!fb_r.multi_file,        "multi_file == false");
        check(fb_r.files.size() == 1,  "1 file");

        const auto* f = findFile(fb_r, "alone.py");
        check(f != nullptr,            "file name == 'alone.py'");
        if (f) {
            check(f->content.find("def alone") != std::string::npos,
                  "content berisi 'def alone'");
        }
    }

    // =====================================================================
    // Test 8 — semua Blocked → 0 written, cache TIDAK invalidate
    // =====================================================================
    std::printf("\n[Test 8] semua Blocked → tidak ada side-effect\n");
    {
        const ImportResult bk_r = imp.preview(blocked_paste);
        check(!bk_r.parse_failed,      "parse_failed == false");
        check(bk_r.files.size() == 2,  "2 file di preview");
        check(bk_r.blocked_count == 2, "blocked_count == 2");
        check(bk_r.new_count == 0,     "new_count == 0");
        check(bk_r.overwrite_count == 0, "overwrite_count == 0");

        const int calls_before = invalidate_calls;
        const bool cache_valid_before = cache.status().valid;

        const ImportResult commit_r = imp.commit();
        check(commit_r.written_count == 0, "written_count == 0");
        check(commit_r.blocked_count == 2, "blocked_count == 2 (diteruskan)");

        // KEY ASSERTION: invalidate TIDAK dipanggil saat tidak ada file
        // yang benar-benar ditulis.
        check(invalidate_calls == calls_before,
              "invalidate_calls TIDAK bertambah");
        check(cache.status().valid == cache_valid_before,
              "cache.valid tidak berubah");
    }

    // =====================================================================
    // Summary
    // =====================================================================
    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    std::printf("sandbox tersisa di: %s\n", sb.root().string().c_str());
    return g_failures == 0 ? 0 : 1;
}
