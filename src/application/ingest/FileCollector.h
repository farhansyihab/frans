// src/application/ingest/FileCollector.h
//
// Fase B #46-47 -- kumpulkan file repo untuk Ingest & Context Pack.
//
// Jalur utama: `git ls-files -z --cached --others --exclude-standard`
//   (honor .gitignore penuh, termasuk untracked file).
// Fallback: std::filesystem walker dengan exclude bawaan kalau bukan
//   git repo atau git tidak tersedia.
//
// Filter (Fase B #47):
//   1. Secret: file dengan basename yang cocok pola rahasia (.env, *.pem, dll)
//   2. Binary: file yang mengandung NUL byte di 8 KB pertama
//   3. Ukuran: file yang melebihi max_file_bytes
//   4. Self-exclude: file di dalam output_dir (untuk menghindari ingest output Frans)
//
// Murni pembacaan disk; tidak menulis apapun.
#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace frans::application {

// Satu file yang terdeteksi di repo.
//
// `skipped = true` berarti file terdaftar (muncul di tree) tapi isinya
// tidak boleh dibaca / tidak boleh dikirim ke AI. `skip_reason` memberi
// tahu kenapa: "secret" | "binary" | "too_large".
struct CollectedFile {
    std::string relative_path;   // forward slash, relatif terhadap project_root
    std::size_t size_bytes = 0;
    bool        is_text    = true;   // false = binary / bukan UTF-8
    bool        skipped    = false;
    std::string skip_reason;         // "secret" | "binary" | "too_large"
};

struct CollectOptions {
    std::size_t max_file_bytes = 256 * 1024;   // 256 KB default
    bool        exclude_secrets = true;
    std::vector<std::string> extra_excludes;   // nama dir / glob basename
    std::string output_dir;                    // di-exclude jika di dalam repo
};

struct CollectResult {
    bool ok = false;
    std::string error;
    bool used_git = false;
    std::vector<CollectedFile> files;   // terurut leksikografis
};

// Kumpulkan file di `project_root`.
//
// - `cancel` boleh nullptr. Kalau non-null dan bernilai true, proses
//   dihentikan dan `result.error = "dibatalkan"`.
// - Return `ok = true` bila traversal selesai (meski beberapa file di-skip).
// - Return `ok = false` bila project_root invalid atau dibatalkan.
CollectResult collectFiles(const std::string& project_root,
                           const CollectOptions& opt,
                           std::atomic<bool>* cancel = nullptr);

// ---------------------------------------------------------------------------
// Helper yang diekspos untuk unit test (murni, tanpa I/O).
// ---------------------------------------------------------------------------

// Apakah `basename` cocok salah satu pola secret bawaan?
bool isSecretBasename(const std::string& basename);

// Apakah `rel_path` (forward slash) harus di-exclude karena komponen
// path-nya masuk daftar exclude bawaan atau `extra_excludes`?
bool isExcludedPath(const std::string& rel_path,
                    const std::vector<std::string>& extra_excludes);

// Apakah konten file bersifat binary (mengandung NUL byte)?
bool isBinaryContent(const std::string& file_path);

} // namespace frans::application