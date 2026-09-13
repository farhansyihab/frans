// ~/AgenAI/frans/tests/sandbox_smoke_test.cpp
//
// Manual smoke test Stage 7a: SandboxedFileSystem.
//
// Buktikan 3 hal (blueprint roadmap 7a):
//   4a. writeFile ke dalam sandbox berhasil, isi bisa dibaca kembali.
//   4b. writeFile dengan path keluar sandbox DITOLAK, tidak ada efek
//       samping di luar sandbox.
//   4c. createDirectory nested otomatis membuat semua folder antara.
//
// Bonus: symlink di dalam sandbox yang menunjuk ke luar juga ditolak.
//
// Jalankan: ./frans_sandbox_test [sandbox_root]
// Default sandbox_root = /tmp/frans_sandbox_test_<pid>

#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>

using frans::infrastructure::SandboxedFileSystem;

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& label) {
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", label.c_str());
    if (!cond) ++g_failures;
}

} // namespace

int main(int argc, char** argv) {
    // ---- Resolve sandbox root ----
    std::string root;
    if (argc > 1) {
        root = argv[1];
    } else {
        root = (fs::temp_directory_path()
                / ("frans_sandbox_test_" + std::to_string(::getpid())))
                .string();
    }

    std::printf("=== frans_ai sandbox smoke test (Stage 7a) ===\n");
    std::printf("root: %s\n\n", root.c_str());

    // Bersihkan dari run sebelumnya supaya hasil deterministik.
    std::error_code ec;
    fs::remove_all(root, ec);

    // Marker path yang HARUS tetap tidak ada kalau sandbox bekerja.
    // Diletakkan di parent root supaya hanya bisa dibuat lewat path escape.
    const fs::path escape_marker =
        fs::path(root).parent_path() / "frans_escape_marker.txt";
    fs::remove(escape_marker, ec);

    SandboxedFileSystem sb(root);
    std::printf("canonical root: %s\n\n", sb.root().string().c_str());

    // =====================================================================
    // 4a. writeFile ke dalam sandbox
    // =====================================================================
    std::printf("[Test 4a] writeFile di dalam sandbox\n");
    {
        const bool ok = sb.writeFile("hello.txt", "hello sandbox\n");
        check(ok,                 "writeFile(\"hello.txt\") -> true");
        check(sb.exists("hello.txt"), "exists(\"hello.txt\") -> true");

        const std::string back = sb.readFile("hello.txt");
        check(back == "hello sandbox\n", "readFile round-trip matches");

        // Verifikasi langsung di filesystem nyata (bukan cuma lewat API).
        check(fs::exists(sb.root() / "hello.txt", ec),
              "file ada di real FS");
    }

    // =====================================================================
    // 4b. Path escape DITOLAK
    // =====================================================================
    std::printf("\n[Test 4b] path escape ditolak\n");
    {
        // (i) "../" relative
        check(!sb.writeFile("../frans_escape_marker.txt", "pwned\n"),
              "writeFile(\"../frans_escape_marker.txt\") -> false");
        check(!fs::exists(escape_marker, ec),
              "marker di luar sandbox TIDAK dibuat");

        // (ii) ".." sebagai target langsung
        check(!sb.writeFile("..", "pwned\n"),
              "writeFile(\"..\") -> false");

        // (iii) absolut ke luar sandbox
        check(!sb.writeFile("/etc/frans_escape_test", "pwned\n"),
              "writeFile(\"/etc/...\") -> false");

        // (iv) nested escape: sub/../../..
        check(!sb.writeFile("sub/../../../frans_escape_marker.txt", "pwned\n"),
              "writeFile(\"sub/../../../...\") -> false");
        check(!fs::exists(escape_marker, ec),
              "marker tetap tidak ada setelah nested escape attempt");

        // (v) symlink di dalam sandbox -> /etc, lalu tulis lewat symlink
        fs::create_directory_symlink("/etc",
                                     sb.root() / "evil_link", ec);
        if (!ec) {
            check(!sb.writeFile("evil_link/frans_escape_via_symlink", "pwned\n"),
                  "writeFile lewat symlink ke /etc -> false");
            check(!fs::exists("/etc/frans_escape_via_symlink", ec),
                  "tidak ada file nyasar di /etc");
        } else {
            std::printf("  SKIP  symlink test (create_directory_symlink: %s)\n",
                        ec.message().c_str());
        }
    }

    // =====================================================================
    // 4c. createDirectory nested
    // =====================================================================
    std::printf("\n[Test 4c] createDirectory nested\n");
    {
        check(!sb.exists("a/b/c/d"), "a/b/c/d belum ada sebelum create");

        const bool ok = sb.createDirectory("a/b/c/d");
        check(ok,                    "createDirectory(\"a/b/c/d\") -> true");
        check(sb.exists("a/b/c/d"),  "exists(\"a/b/c/d\") -> true");
        check(fs::is_directory(sb.root() / "a" / "b" / "c" / "d", ec),
              "real FS: a/b/c/d adalah directory");

        // Tulis file di dalam nested dir — pastikan resolveSafe tetap benar.
        const bool wok = sb.writeFile("a/b/c/d/nested.txt", "nested\n");
        check(wok, "writeFile ke nested dir -> true");
        check(sb.readFile("a/b/c/d/nested.txt") == "nested\n",
              "round-trip nested file matches");
    }

    // =====================================================================
    // Sanity: cleanup idempotent
    // =====================================================================
    std::printf("\n[Sanity] deleteFile idempotent\n");
    {
        check(sb.deleteFile("hello.txt"),         "hapus hello.txt -> true");
        check(sb.deleteFile("hello.txt"),         "hapus ulang -> tetap true");
        check(!sb.exists("hello.txt"),            "hello.txt sudah tidak ada");
        check(!sb.deleteFile("../../etc/passwd"), "deleteFile escape -> false");
    }

    std::printf("\n=== %s ===\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    std::printf("sandbox tersisa di: %s\n", sb.root().string().c_str());
    return g_failures == 0 ? 0 : 1;
}
