// src/application/context/SourceCodeBuilder.h
//
// Fase C #60 + #61 -- SourceCodeBuilder (BluePrint v9 Addendum §4.6)
//
// Tugas:
//   - Membangun SourceCode.md dari daftar file terpilih.
//   - Struktur folder lengkap (semua file hasil collectFiles tanpa secret).
//   - File terpilih ditandai dengan "*" di tree.
//   - Isi file dibaca langsung dari disk (bukan dari Gemini).
//   - Menulis manifest JSON (hash FNV-1a 64-bit per KONTEN file + hash request).
//
// Komponen:
//   - MarkdownRenderer: render tree + file blocks.
//   - SandboxedFileSystem: baca file dari disk.
//   - FNV-1a 64-bit hash: untuk manifest (hash konten, bukan path).
//
// Murni I/O disk; tidak mengakses jaringan atau UI.
#pragma once
#include "application/context/ContextSelector.h"
#include "application/ingest/FileCollector.h"
#include "application/ingest/MarkdownRenderer.h"
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>
namespace frans::application {
// Metadata untuk manifest.
struct BuildManifest {
std::string created_at;           // Timestamp ISO-like (sudah di-format caller)
std::string request_hash;         // FNV-1a 64-bit hex dari request_text
std::string mode;                 // "full" | "index"
std::string model;                // Nama model Gemini yang dipakai
};
// Info hash per file untuk manifest.
struct FileHashInfo {
std::string path;
std::string content_hash;         // FNV-1a 64-bit hex dari ISI file
std::size_t size_bytes = 0;
};
// Hasil build SourceCode.md.
struct BuildResult {
bool ok = false;
std::string error;
std::string source_code_path;     // Path absolut ke SourceCode.md
std::string manifest_path;        // Path absolut ke SourceCode.manifest.json
std::size_t total_bytes = 0;      // Total byte SourceCode.md
std::size_t files_included = 0;   // Jumlah file yang isinya disertakan
};
/**
* @brief Bangun SourceCode.md dari daftar file terpilih.
*
* @param all_files Semua file di repo (dari collectFiles, tanpa secret).
* @param selected_files Daftar file terpilih (dari ContextSelector).
* @param project_root Root proyek (untuk SandboxedFileSystem).
* @param output_dir Direktori output (SourceCode.md + manifest).
* @param context_output_name Nama file output (mis. "SourceCode.md").
* @param request_text Permintaan pengguna (untuk hash manifest).
* @param manifest Metadata manifest (mode, model, timestamp).
* @param cancel Flag pembatalan (opsional).
* @return BuildResult berisi path file output dan metadata.
*/
BuildResult buildSourceCode(
const std::vector<CollectedFile>& all_files,
const std::vector<ContextFileEntry>& selected_files,
const std::string& project_root,
const std::string& output_dir,
const std::string& context_output_name,
const std::string& request_text,
const BuildManifest& manifest,
std::atomic<bool>* cancel = nullptr);
// ---------------------------------------------------------------------------
// Helper yang diekspos untuk unit test (murni, tanpa I/O).
// ---------------------------------------------------------------------------
// Hitung hash FNV-1a 64-bit dari string.
// Return hex string 16 karakter (lowercase).
std::string fnv1a_64_hex(const std::string& data);
// Bangun JSON manifest (murni, tanpa I/O).
std::string buildManifestJson(
const BuildManifest& manifest,
const std::vector<FileHashInfo>& file_hashes);
} // namespace frans::application