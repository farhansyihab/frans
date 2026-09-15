// ~/AgenAI/frans/tests/manual_importer_smoke_test.cpp
//
// Manual smoke test Stage 11a/11b: ManualImporter.
//
// Cakupan (17 test):
//   --- Stage 11a (basic) ---
//   1.  Preview single-file fallback (tanpa marker File:)
//   2.  Preview multi-file (marker File: + fenced block)
//   3.  Preview path escape → Blocked
//   4.  Preview danger check (NeedsConfirm vs Blocked)
//   5.  Preview Overwrite detection (file sudah ada)
//   6.  Commit tulis file baru + snapshot kosong
//   7.  Commit skip Blocked
//   8.  Commit overwrite + snapshot isi lama
//   9.  Commit tanpa preview → parse_failed
//   10. Invalidation callback terpanggil sekali
//   11. Preview input kosong → parse_failed
//   12. Preview tanpa marker + tanpa fallback_path → parse_failed
//   --- Stage 11b-fix (subfolder + leading slash) ---
//   13. Preview subfolder path → New (bukan Blocked)
//   14. Commit subfolder → parent dir dibuat otomatis
//   15. Preview leading slash → dinormalisasi, bukan Blocked
//   16. Commit deep-nested (a/b/c/d.py) → semua parent dibuat
//   17. Leading slash + escape ('/../evil.py') → tetap Blocked
//
// Jalankan: ./frans_manual_import_test [sandbox_root]
// Default sandbox_root = /tmp/frans_manual_import_test_<pid>

#include "application/manual_import/ManualImporter.h"
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>

using frans::application::DangerVerdict;
using frans::application::ImportFileStatus;
using frans::application::ImportFilePreview;
using frans::application::ImportResult;
using frans::application::ManualImporter;
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
                / ("frans_manual_import_test_" + std::to_string(::getpid())))
                .string();
    }

    std::printf("=== frans_ai manual import smoke test (Stage 11a/11b) ===\n");
    std::printf("root: %s\n\n", root.c_str());

    std::error_code ec;
    fs::remove_all(root, ec);

    SandboxedFileSystem sb(root);
    std::printf("canonical root: %s\n\n", sb.root().string().c_str());

    // =====================================================================
    // Test 1: preview single-file fallback
    // =====================================================================
    std::printf("[Test 1] preview single-file (fallback path)\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "```python\n"
            "print('hello')\n"
            "```\n";
        const auto r = imp.preview(text, "hello.py");

        check(!r.parse_failed, "parse_failed == false");
        check(r.files.size() == 1, "1 file");
        check(r.multi_file == false, "multi_file == false");

        const auto* f = findFile(r, "hello.py");
        check(f != nullptr, "file hello.py ada di preview");
        if (f) {
            check(f->status == ImportFileStatus::New, "status == New");
            check(f->content.find("print") != std::string::npos,
                  "content berisi 'print'");
            check(f->size_bytes > 0, "size_bytes > 0");
        }
        check(r.new_count == 1, "new_count == 1");
        check(r.blocked_count == 0, "blocked_count == 0");
        check(imp.hasPreview(), "hasPreview() == true");
    }

    // =====================================================================
    // Test 2: preview multi-file
    // =====================================================================
    std::printf("\n[Test 2] preview multi-file\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "File: a.py\n"
            "```python\n"
            "a = 1\n"
            "```\n"
            "\n"
            "File: sub/b.py\n"
            "```python\n"
            "b = 2\n"
            "```\n";
        const auto r = imp.preview(text);

        check(!r.parse_failed, "parse_failed == false");
        check(r.files.size() == 2, "2 files");
        check(r.multi_file == true, "multi_file == true");

        const auto* a = findFile(r, "a.py");
        const auto* b = findFile(r, "sub/b.py");
        check(a != nullptr, "a.py ada");
        check(b != nullptr, "sub/b.py ada");
        if (a) check(a->content == "a = 1", "a.py content == 'a = 1'");
        if (b) check(b->content == "b = 2", "b.py content == 'b = 2'");
    }

    // =====================================================================
    // Test 3: path escape → Blocked
    // =====================================================================
    std::printf("\n[Test 3] path escape → Blocked\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "File: ../escape.py\n"
            "```python\n"
            "print('pwned')\n"
            "```\n";
        const auto r = imp.preview(text);

        check(!r.parse_failed, "parse_failed == false");
        check(r.files.size() == 1, "1 file");

        const auto* f = findFile(r, "../escape.py");
        check(f != nullptr, "file ada di preview");
        if (f) {
            check(f->status == ImportFileStatus::Blocked,
                  "status == Blocked");
            check(!f->blocked_reason.empty(),
                  "blocked_reason tidak kosong");
        }
        check(r.blocked_count == 1, "blocked_count == 1");
        check(r.new_count == 0, "new_count == 0");
    }

    // =====================================================================
    // Test 4: danger check
    // =====================================================================
    std::printf("\n[Test 4] danger check (python)\n");
    {
        ManualImporter imp(sb);

        // 4a. os.system → NeedsConfirm (tidak block, tetap New)
        {
            const std::string text =
                "File: risky.py\n"
                "```python\n"
                "import os\n"
                "os.system('ls')\n"
                "```\n";
            const auto r = imp.preview(text);
            const auto* f = findFile(r, "risky.py");
            check(f != nullptr, "4a: file risky.py ada");
            if (f) {
                check(f->status == ImportFileStatus::New,
                      "4a: status == New (NeedsConfirm tidak block)");
                check(f->danger == DangerVerdict::NeedsConfirm,
                      "4a: danger == NeedsConfirm");
            }
            check(r.blocked_count == 0, "4a: blocked_count == 0");
        }

        // 4b. /etc/passwd literal → Blocked
        {
            const std::string text =
                "File: evil.py\n"
                "```python\n"
                "print(open('/etc/passwd').read())\n"
                "```\n";
            const auto r = imp.preview(text);
            const auto* f = findFile(r, "evil.py");
            check(f != nullptr, "4b: file evil.py ada");
            if (f) {
                check(f->status == ImportFileStatus::Blocked,
                      "4b: status == Blocked");
                check(f->danger == DangerVerdict::Blocked,
                      "4b: danger == Blocked");
            }
            check(r.blocked_count == 1, "4b: blocked_count == 1");
        }
    }

    // =====================================================================
    // Test 5: overwrite detection
    // =====================================================================
    std::printf("\n[Test 5] preview Overwrite\n");
    {
        // Siapkan file yang sudah ada.
        const bool w = sb.writeFile("existing.py", "old content\n");
        check(w, "setup: write existing.py");

        ManualImporter imp(sb);
        const std::string text =
            "File: existing.py\n"
            "```python\n"
            "new content\n"
            "```\n";
        const auto r = imp.preview(text);
        const auto* f = findFile(r, "existing.py");
        check(f != nullptr, "file existing.py ada");
        if (f) {
            check(f->status == ImportFileStatus::Overwrite,
                  "status == Overwrite");
        }
        check(r.overwrite_count == 1, "overwrite_count == 1");
        check(r.new_count == 0, "new_count == 0");
    }

    // =====================================================================
    // Test 6: commit (file baru)
    // =====================================================================
    std::printf("\n[Test 6] commit file baru + snapshot kosong\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "File: c1.py\n"
            "```python\n"
            "c1 = 1\n"
            "```\n"
            "File: c2.py\n"
            "```python\n"
            "c2 = 2\n"
            "```\n";
        const auto prev = imp.preview(text);
        check(prev.files.size() == 2, "preview: 2 files");
        check(prev.new_count == 2, "preview: new_count == 2");

        const auto res = imp.commit();
        check(res.written_count == 2, "commit: written_count == 2");
        check(res.failed_count == 0, "commit: failed_count == 0");

        check(sb.exists("c1.py"), "c1.py ada di disk");
        check(sb.exists("c2.py"), "c2.py ada di disk");
        check(sb.readFile("c1.py") == "c1 = 1", "c1.py content match");
        check(sb.readFile("c2.py") == "c2 = 2", "c2.py content match");

        // Snapshot untuk file baru = string kosong.
        const auto& snaps = imp.snapshots();
        check(snaps.count("c1.py") == 1, "snapshot c1.py ada");
        check(snaps.count("c2.py") == 1, "snapshot c2.py ada");
        if (snaps.count("c1.py")) {
            check(snaps.at("c1.py").empty(),
                  "snapshot c1.py kosong (file baru)");
        }

        check(!imp.hasPreview(), "hasPreview() == false setelah commit");
    }

    // =====================================================================
    // Test 7: commit skip Blocked
    // =====================================================================
    std::printf("\n[Test 7] commit skip Blocked\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "File: ok.py\n"
            "```python\n"
            "ok = True\n"
            "```\n"
            "File: ../evil.py\n"
            "```python\n"
            "print('escape')\n"
            "```\n";
        const auto prev = imp.preview(text);
        check(prev.files.size() == 2, "preview: 2 files");
        check(prev.blocked_count == 1, "preview: 1 blocked");

        const auto res = imp.commit();
        check(res.written_count == 1, "commit: written_count == 1");
        check(res.failed_count == 0, "commit: failed_count == 0");
        check(res.blocked_count == 1, "commit: blocked_count == 1");

        check(sb.exists("ok.py"), "ok.py ditulis");
        check(!sb.exists("../evil.py"),
              "evil.py TIDAK ditulis (blocked)");
    }

    // =====================================================================
    // Test 8: commit overwrite + snapshot isi lama
    // =====================================================================
    std::printf("\n[Test 8] commit overwrite + snapshot isi lama\n");
    {
        // Setup: file dengan content lama.
        sb.writeFile("ovr.py", "OLD\n");

        ManualImporter imp(sb);
        const std::string text =
            "File: ovr.py\n"
            "```python\n"
            "NEW\n"
            "```\n";
        imp.preview(text);
        const auto res = imp.commit();
        check(res.written_count == 1, "written_count == 1");

        check(sb.readFile("ovr.py") == "NEW",
              "file content == NEW setelah overwrite");

        const auto& snaps = imp.snapshots();
        check(snaps.count("ovr.py") == 1, "snapshot ovr.py ada");
        if (snaps.count("ovr.py")) {
            check(snaps.at("ovr.py") == "OLD\n",
                  "snapshot berisi 'OLD\\n' (isi lama)");
        }
    }

    // =====================================================================
    // Test 9: commit tanpa preview → error
    // =====================================================================
    std::printf("\n[Test 9] commit tanpa preview\n");
    {
        ManualImporter imp(sb);
        const auto res = imp.commit();
        check(res.parse_failed, "parse_failed == true");
        check(!res.parse_error.empty(), "parse_error terisi");
        check(res.written_count == 0, "written_count == 0");
    }

    // =====================================================================
    // Test 10: invalidation callback
    // =====================================================================
    std::printf("\n[Test 10] invalidation callback\n");
    {
        int invalidations = 0;
        ManualImporter imp(sb, [&invalidations]{ ++invalidations; });

        imp.preview("File: cb.py\n```python\ncb = 1\n```\n");
        imp.commit();
        check(invalidations == 1, "callback terpanggil 1x setelah commit");

        // Commit kedua tanpa preview sukses → tidak memicu callback.
        imp.commit();
        check(invalidations == 1, "callback tidak terpanggil lagi");
    }

    // =====================================================================
    // Test 11: preview input kosong → parse_failed
    // =====================================================================
    std::printf("\n[Test 11] preview input kosong\n");
    {
        ManualImporter imp(sb);
        const auto r = imp.preview("");
        check(r.parse_failed, "parse_failed == true");
        check(!r.parse_error.empty(), "parse_error terisi");
        check(!imp.hasPreview(), "hasPreview() == false");
    }

    // =====================================================================
    // Test 12: preview tanpa marker + tanpa fallback_path → error
    // =====================================================================
    std::printf("\n[Test 12] preview tanpa marker + tanpa fallback\n");
    {
        ManualImporter imp(sb);
        const auto r = imp.preview("```python\nprint(1)\n```\n", "");
        check(r.parse_failed, "parse_failed == true");
        check(!r.parse_error.empty(), "parse_error terisi");
    }

    // =====================================================================
    // Test 13: preview subfolder path (parent belum ada)
    // =====================================================================
    std::printf("\n[Test 13] preview subfolder path (parent belum ada)\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "File: sub_pkg/module.py\n"
            "```python\n"
            "x = 1\n"
            "```\n";
        const auto r = imp.preview(text);

        check(!r.parse_failed, "parse_failed == false");
        check(r.files.size() == 1, "1 file");

        const auto* f = findFile(r, "sub_pkg/module.py");
        check(f != nullptr, "file sub_pkg/module.py ada di preview");
        if (f) {
            // HARUS New, bukan Blocked — path valid di dalam sandbox
            // meskipun parent belum ada.
            check(f->status == ImportFileStatus::New,
                  "status == New (bukan Blocked)");
        }
        check(r.blocked_count == 0, "blocked_count == 0");
        check(r.new_count == 1, "new_count == 1");
    }

    // =====================================================================
    // Test 14: commit subfolder — parent dir dibuat otomatis
    // =====================================================================
    std::printf("\n[Test 14] commit subfolder — parent dir auto-create\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "File: auto_pkg/deep_file.py\n"
            "```python\n"
            "auto = True\n"
            "```\n";
        imp.preview(text);
        const auto res = imp.commit();

        check(res.written_count == 1, "written_count == 1");
        check(res.failed_count  == 0, "failed_count == 0");
        check(sb.exists("auto_pkg"),              "parent dir 'auto_pkg' ada");
        check(sb.exists("auto_pkg/deep_file.py"), "file ada di disk");
        check(sb.readFile("auto_pkg/deep_file.py") == "auto = True",
              "content match");
    }

    // =====================================================================
    // Test 15: leading slash di marker path → dinormalisasi
    // =====================================================================
    std::printf("\n[Test 15] leading slash dinormalisasi\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "File: /pkg_lead/mod.py\n"
            "```python\n"
            "lead = 1\n"
            "```\n";
        const auto r = imp.preview(text);

        check(!r.parse_failed, "parse_failed == false");
        check(r.files.size() == 1, "1 file");

        // Path HARUS sudah dinormalisasi (tanpa leading slash).
        const auto* f = findFile(r, "pkg_lead/mod.py");
        check(f != nullptr, "path ternormalisasi ke 'pkg_lead/mod.py'");

        const auto* raw = findFile(r, "/pkg_lead/mod.py");
        check(raw == nullptr,
              "path asli dengan leading slash tidak ada di preview");

        if (f) {
            check(f->status == ImportFileStatus::New,
                  "status == New (leading slash tidak bikin Blocked)");
        }
        check(r.blocked_count == 0, "blocked_count == 0");
    }

    // =====================================================================
    // Test 16: commit deep-nested path
    // =====================================================================
    std::printf("\n[Test 16] commit deep-nested path (a/b/c/d.py)\n");
    {
        ManualImporter imp(sb);
        const std::string text =
            "File: a/b/c/d.py\n"
            "```python\n"
            "deep = True\n"
            "```\n";
        imp.preview(text);
        const auto res = imp.commit();

        check(res.written_count == 1, "written_count == 1");
        check(res.failed_count == 0,  "failed_count == 0");
        check(sb.exists("a"),          "a/ ada");
        check(sb.exists("a/b"),        "a/b/ ada");
        check(sb.exists("a/b/c"),      "a/b/c/ ada");
        check(sb.exists("a/b/c/d.py"), "a/b/c/d.py ada");
        check(sb.readFile("a/b/c/d.py") == "deep = True", "content match");
    }

    // =====================================================================
    // Test 17: leading slash + escape tetap ditolak
    // =====================================================================
    std::printf("\n[Test 17] leading slash + escape tetap ditolak\n");
    {
        ManualImporter imp(sb);
        // Setelah normalize: "/../evil_lead.py" -> "../evil_lead.py" -> Blocked.
        // Ini mengunci invariant keamanan: normalisasi leading slash TIDAK
        // membuka celah untuk escape via '..'.
        const std::string text =
            "File: /../evil_lead.py\n"
            "```python\n"
            "x = 1\n"
            "```\n";
        const auto r = imp.preview(text);

        check(r.files.size() == 1, "1 file");
        check(r.blocked_count == 1, "blocked_count == 1");

        const auto* f = findFile(r, "../evil_lead.py");
        check(f != nullptr, "path ternormalisasi ke '../evil_lead.py'");
        if (f) {
            check(f->status == ImportFileStatus::Blocked,
                  "status == Blocked (escape via '..' tetap dicegah)");
        }
    }

    std::printf("\n=== %s ===\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    std::printf("sandbox tersisa di: %s\n", sb.root().string().c_str());
    return g_failures == 0 ? 0 : 1;
}
