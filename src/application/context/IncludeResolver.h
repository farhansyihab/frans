// src/application/context/IncludeResolver.h
//
// Fase E #74 -- Local include closure (BluePrint v9 Addendum §9, Fase E).
//
// Tugas:
//   Melengkapi seleksi Gemini dengan file lokal yang di-`#include "..."`
//   oleh file terpilih (C/C++), secara transitif sampai kedalaman terbatas.
//   Contoh: Gemini memilih `ManualImportDialog.cpp`, tetapi lupa
//   `ManualImportDialog.h` -> closure menambahkannya (dan header yang
//   di-include header itu, dst).
//
// Aturan:
//   - Hanya `#include "..."` (kutip ganda). `#include <...>` diabaikan.
//   - Komentar (// dan /* */) diabaikan. `#if 0` TIDAK dievaluasi.
//   - Target hanya diambil dari daftar file hasil collectFiles()
//     (tanpa secret/binary/terlalu besar), jadi path halusinasi dan file
//     rahasia tidak mungkin masuk.
//   - Urutan resolusi path (mirip compiler):
//       1. relatif terhadap folder file yang meng-include
//       2. relatif terhadap tiap `search_dirs` (mis. "src", "include")
//       3. suffix match unik di seluruh repo (opsional, `allow_suffix_match`)
//   - Dibatasi: kedalaman, jumlah file tambahan, total byte tambahan.
//   - Opsional: `enabled = false` secara default (toggle).
//
// Murni baca disk (via SandboxedFileSystem); tanpa jaringan, tanpa UI.
#pragma once

#include "application/ingest/FileCollector.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace frans::application {

struct IncludeClosureOptions {
    bool        enabled            = false;       // toggle; false = tidak melakukan apa-apa
    int         max_depth          = 3;           // 1 = hanya include langsung dari file terpilih
    std::size_t max_added_files    = 20;          // batas jumlah file tambahan
    std::size_t max_added_bytes    = 100 * 1024;  // batas total byte file tambahan
    bool        allow_suffix_match = true;        // langkah 3 (suffix match unik)
    std::vector<std::string> search_dirs = {"src", "include"};   // relatif project_root
};

// Satu file yang ditambahkan oleh closure.
struct IncludeAddition {
    std::string path;           // relatif project_root, forward slash
    std::string included_by;    // file yang meng-include
    std::string directive;      // teks di dalam "..." persis seperti ditulis
    int         depth = 1;      // 1 = di-include langsung oleh file terpilih
    std::size_t size_bytes = 0;
};

// Include yang tidak bisa diselesaikan (untuk info di UI, bukan error).
struct IncludeUnresolved {
    std::string included_by;
    std::string directive;
    std::string reason;         // "not_found" | "ambiguous (N kandidat)" | "skipped:<alasan>"
};

struct IncludeClosureResult {
    bool ok = false;
    std::string error;                              // mis. "dibatalkan"
    std::vector<IncludeAddition>  additions;        // urutan deterministik (BFS)
    std::vector<IncludeUnresolved> unresolved;      // dibatasi maks. 200 entri
    std::vector<std::string>      dropped_budget;   // ada di repo, tapi melewati batas
    std::size_t added_bytes = 0;
};

// Pembaca isi file (relatif project_root). Return false bila file tidak ada.
using IncludeFileReader =
    std::function<bool(const std::string& rel_path, std::string& out_content)>;

// Versi inti dengan pembaca file yang bisa diganti (dipakai test in-memory).
IncludeClosureResult resolveIncludeClosureWith(
    const IncludeFileReader& reader,
    const std::vector<std::string>& seed_paths,
    const std::vector<CollectedFile>& all_files,
    const IncludeClosureOptions& opt,
    std::atomic<bool>* cancel = nullptr);

// Versi produksi: membaca isi file lewat SandboxedFileSystem(project_root).
//
// - `seed_paths`  : file yang sudah terpilih (path relatif, forward slash).
// - `all_files`   : hasil collectFiles() (sumber kebenaran file yang boleh).
// - `cancel`      : boleh nullptr; true -> ok=false, error="dibatalkan".
// - opt.enabled=false -> return ok=true dengan hasil kosong.
IncludeClosureResult resolveIncludeClosure(
    const std::string& project_root,
    const std::vector<std::string>& seed_paths,
    const std::vector<CollectedFile>& all_files,
    const IncludeClosureOptions& opt,
    std::atomic<bool>* cancel = nullptr);

// Ringkasan satu baris untuk label/notes di UI.
// Contoh: "Include closure: +3 file (12.4 KB), 1 include tidak terselesaikan"
std::string summarizeIncludeClosure(const IncludeClosureResult& r);

// ---------------------------------------------------------------------------
// Helper yang diekspos untuk unit test (murni, tanpa I/O).
// ---------------------------------------------------------------------------

// Ambil semua target `#include "..."` dari isi file, sesuai urutan kemunculan.
std::vector<std::string> parseQuotedIncludes(const std::string& content);

// Normalisasi path relatif: backslash -> '/', buang "." dan "//", lipat "..".
// Return nullopt bila path kosong, absolut (/x, C:\x), atau keluar dari root.
// Return "" (string kosong) bila hasilnya root itu sendiri (mis. ".").
std::optional<std::string> normalizeRelPath(const std::string& path);

// Apakah ekstensi `path` layak dipindai untuk `#include` (C/C++ source/header)?
bool isIncludeScannable(const std::string& path);

// ---------------------------------------------------------------------------
// Integrasi dengan daftar seleksi.
//
// Dibuat template agar tidak terikat bentuk persis ContextFileEntry /
// SelectedFile. Syarat minimal Entry: default-constructible, punya
// `std::string path` dan `bool selected`. Field opsional diisi hanya bila ada:
// `std::string priority`, `std::string reason`, `std::size_t size_bytes`.
// ---------------------------------------------------------------------------

// Tambahkan file hasil closure ke `entries` (dilewati bila path sudah ada,
// termasuk yang di-uncheck pengguna). Return jumlah entri baru.
template <typename Entry>
std::size_t mergeIncludeClosure(std::vector<Entry>& entries,
                                const IncludeClosureResult& result)
{
    std::unordered_set<std::string> existing;
    for (const auto& e : entries) existing.insert(e.path);

    std::size_t added = 0;
    for (const auto& add : result.additions) {
        if (existing.count(add.path) != 0) continue;

        Entry e{};
        e.path     = add.path;
        e.selected = true;
        if constexpr (requires { e.priority = std::string(); }) {
            e.priority = "helpful";
        }
        if constexpr (requires { e.reason = std::string(); }) {
            e.reason = "#include \"" + add.directive + "\" dari " + add.included_by;
        }
        if constexpr (requires { e.size_bytes = std::size_t{}; }) {
            e.size_bytes = add.size_bytes;
        }
        entries.push_back(std::move(e));
        existing.insert(add.path);
        ++added;
    }
    return added;
}

// Satu panggilan: ambil seed dari entri yang `selected`, jalankan closure,
// lalu gabungkan hasilnya ke `entries`. Bila `result.ok == false`
// (mis. dibatalkan), `entries` tidak diubah.
template <typename Entry>
IncludeClosureResult applyIncludeClosure(std::vector<Entry>& entries,
                                         const std::string& project_root,
                                         const std::vector<CollectedFile>& all_files,
                                         const IncludeClosureOptions& opt,
                                         std::atomic<bool>* cancel = nullptr)
{
    std::vector<std::string> seeds;
    for (const auto& e : entries) {
        if (e.selected) seeds.push_back(e.path);
    }
    IncludeClosureResult r =
        resolveIncludeClosure(project_root, seeds, all_files, opt, cancel);
    if (r.ok) mergeIncludeClosure(entries, r);
    return r;
}

} // namespace frans::application