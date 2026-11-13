// src/application/ingest/MarkdownRenderer.h
//
// Fase B #48 -- render tree + file blocks, format ala gitingest.
// Reusable untuk:
//   - RepoIngester (Fase B #50): ingest penuh seluruh repo
//   - SourceCodeBuilder (Fase C #60): tree lengkap + isi file terpilih
//
// Output format:
//   # <header_title>: <repo_name>
//   Dibuat oleh Frans | <timestamp> | <N> file disertakan | <size>
//   Permintaan: <ringkasan>
//   Tanda "*" pada struktur di bawah = isi file disertakan setelah bagian ini.
//
//   Directory structure:
//   └── <repo_name>/
//       ├── file1.txt *
//       ├── src/
//       │   └── main.cpp *
//       └── README.md
//
//   ================================================
//   FILE: file1.txt
//   ================================================
//   <isi file>
//
// Murni fungsi teks: tidak mengakses filesystem, jaringan, atau UI.
#pragma once

#include "application/ingest/FileCollector.h"
#include <map>
#include <string>
#include <vector>

namespace frans::application {

// Opsi render untuk header markdown.
struct RenderOptions {
    std::string repo_name;            // nama repo (untuk header & tree root)
    std::string header_title;         // mis. "SourceCode.md" atau "Ingest"
    std::string created_at;           // timestamp string (sudah di-format caller)
    std::string request_summary;      // ringkasan permintaan (saran: ≤ 200 char)
    std::size_t total_content_bytes = 0;   // total byte konten yang disertakan
    std::size_t total_files_included = 0;  // jumlah file yang isinya disertakan
};

// Input untuk renderMarkdown.
struct RenderInput {
    // Semua file (untuk tree). Biasanya hasil collectFiles tanpa secret.
    // Urutan tidak perlu di-sort; renderer akan mengurutkan.
    std::vector<CollectedFile> all_files;

    // Path file yang isinya disertakan (ditandai "*" di tree).
    // Harus subset dari all_files (path yang tidak ada di all_files
    // diabaikan untuk tree, tapi tetap dirender di file blocks bila
    // kontennya ada di file_contents).
    std::vector<std::string> starred_paths;

    // Konten tiap file (key = relative_path).
    // Dipisah dari all_files supaya CollectedFile tetap ringan
    // (tidak perlu menyalin konten besar ke struct).
    std::map<std::string, std::string> file_contents;

    RenderOptions options;
};

// Render markdown lengkap: header + tree + file blocks.
//
// Urutan file blocks mengikuti urutan starred_paths (setelah di-sort
// leksikografis internal supaya deterministik).
std::string renderMarkdown(const RenderInput& in);

// ---------------------------------------------------------------------------
// Helper yang diekspos untuk unit test (murni, tanpa I/O).
// ---------------------------------------------------------------------------

// Render tree direktori saja.
//
// `starred_paths` = daftar path yang ditandai dengan `*` (isi disertakan).
// Tree di-sort: alphabetical murni (file dan dir bercampur), dengan
// konvensi gitingest.
std::string renderTree(const std::vector<CollectedFile>& files,
                       const std::string& root_name,
                       const std::vector<std::string>& starred_paths);

// Render satu file block (separator + FILE: path + separator + konten).
std::string renderFileBlock(const std::string& path,
                            const std::string& content);

// Deteksi bahasa dari ekstensi file (untuk fenced code block bila
// dipakai di konteks lain, mis. prompt template).
// Return "" kalau tidak dikenal.
std::string detectLanguageFromExtension(const std::string& path);

// Format byte ke string human-readable (B / KB / MB).
std::string formatBytesHuman(std::size_t bytes);

} // namespace frans::application