// ~/AgenAI/frans/src/application/manual_import/ManualImporter.h
#pragma once

#include "application/agent/MultiFileParser.h"        // GeneratedFile
#include "application/safety/DangerCheckHandler.h"    // DangerCheckHandler, DangerVerdict
#include "domain/fs/IFileSystemPort.h"

#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace frans::application {

// Blueprint Stage 11a — Manual Import Module (application layer).
//
// ManualImporter adalah otak dari Manual Import Window (Stage 11b).
// Tugasnya:
//   1. Parse teks mentah dari chat AI browser menjadi vector<GeneratedFile>
//      (reuse MultiFileParser — Stage 7e).
//   2. Validasi setiap file:
//        - sandbox containment (IFileSystemPort::isPathSafe)
//        - danger check (DangerCheckHandler — Stage 7c)
//        - deteksi file baru vs overwrite (IFileSystemPort::exists)
//   3. Tulis file ke disk dengan snapshot-before-write (untuk diff/rollback).
//
// Desain 2-phase (WAJIB, blueprint §5.9):
//
//   Phase 1 — preview(text)
//     Parse + validasi, TIDAK menulis apapun. Return ImportResult berisi
//     daftar ImportFilePreview dengan status New/Overwrite/Blocked.
//
//   Phase 2 — commit()
//     Tulis semua file yang status-nya bukan Blocked. Capture snapshot
//     sebelum write. Panggil invalidation callback (biasanya
//     ProjectGraphCache::invalidate) supaya graph rebuild.
//
//   reset()
//     Buang state preview + snapshot. UI panggil setelah dialog close.
//
// Keputusan desain non-obvious:
//
//   * Invalidation callback (std::function<void()>), bukan pointer ke
//     ProjectGraphCache. Alasan: mematuhi Dependency Inversion — layer
//     application tidak perlu tahu tentang implementasi cache konkret.
//     Caller (UI) yang wiring. Bonus: test tidak perlu link tree-sitter.
//
//   * Danger verdict NeedsConfirm TIDAK memblokir commit. Konteks Manual
//     Import adalah user-mediated — user sudah melihat preview dan klik
//     "Write to Disk", itu adalah bentuk konfirmasi eksplisit. Yang
//     diblokir hanya:
//       1. Path escape (sandbox reject) — bukan urusan user, ini safety.
//       2. Danger verdict == Blocked — path sistem kritis seperti
//          /etc/passwd. Blocked mutlak, tidak ada override.
//
//   * Snapshot diambil per file SEBELUM write, disimpan di map internal.
//     Idempotent — kalau file yang sama muncul dua kali di satu preview
//     (jarang, tapi mungkin dengan input malformed), snapshot tetap versi
//     paling awal. Konsisten dengan snapshotBeforeWrite() AgentContext.h.
//
//   * Stateless antar preview: preview() berikutnya mereset seluruh state.
//     Tidak ada akumulasi snapshot lintas preview.

// Status satu file dalam preview.
//
//   New       : file belum ada, akan dibuat.
//   Overwrite : file sudah ada, akan ditimpa.
//   Blocked   : ditolak (path tidak aman / danger check blocked).
//               TIDAK akan ditulis saat commit().
enum class ImportFileStatus {
    New,
    Overwrite,
    Blocked,
};

// Preview satu file (Phase 1). Content tidak ditulis sampai commit.
struct ImportFilePreview {
    std::string       relative_path;
    std::string       content;
    std::size_t       size_bytes = 0;
    ImportFileStatus  status     = ImportFileStatus::New;

    // Hasil DangerCheckHandler::checkCode(). Safe/NeedsConfirm/Blocked.
    // Untuk path escape (status=Blocked karena sandbox), field ini
    // di-set Blocked juga supaya konsisten secara semantik.
    DangerVerdict     danger     = DangerVerdict::Safe;

    // Guest-friendly penjelasan danger (dari DangerCheckHandler), atau
    // kosong kalau verdict Safe.
    std::string       danger_explanation;

    // Terisi kalau status == Blocked. Sumber: sandbox reject, atau
    // DangerCheckHandler block. UI pakai ini sebagai tooltip alasan.
    std::string       blocked_reason;
};

// Hasil satu fase (preview atau commit).
//
// Preview mengisi: files, parse_failed, parse_error, multi_file,
//                  total_bytes, new_count, overwrite_count, blocked_count.
// Commit  mengisi field yang sama PLUS written_count, failed_count.
struct ImportResult {
    std::vector<ImportFilePreview> files;

    bool        parse_failed = false;
    std::string parse_error;
    bool        multi_file   = false;

    std::size_t total_bytes     = 0;   // total byte file yang AKAN ditulis
                                       // (kecuali blocked)
    std::size_t new_count       = 0;
    std::size_t overwrite_count = 0;
    std::size_t blocked_count   = 0;

    // Hanya terisi setelah commit().
    std::size_t written_count = 0;
    std::size_t failed_count  = 0;

    bool has_blocked() const { return blocked_count > 0; }
    bool has_writable() const {
        return new_count + overwrite_count > 0;
    }
    bool is_empty() const { return files.empty(); }
};

class ManualImporter {
public:
    // Callback dipanggil setelah commit berhasil menulis minimal satu file.
    // Caller bisa wire ke ProjectGraphCache::invalidate, atau no-op kalau
    // tidak ada cache. Dipanggil TEPAT SEKALI per commit (bukan per file).
    using InvalidateFn = std::function<void()>;

    ManualImporter(frans::domain::IFileSystemPort& fs,
                   InvalidateFn on_written = nullptr);

    ManualImporter(const ManualImporter&)            = delete;
    ManualImporter& operator=(const ManualImporter&) = delete;

    // ---- Phase 1 ----
    //
    // Parse + validasi. `fallback_path` dipakai kalau `text` tidak punya
    // marker `File:` (single-file mode). Kalau fallback_path kosong DAN
    // tidak ada marker, parse gagal (parse_failed=true).
    //
    // Return snapshot dari state internal. Caller TIDAK boleh simpan
    // reference ke field `files` — panggil preview() berikutnya
    // menginvalidasi isi vector.
    ImportResult preview(const std::string& text,
                         const std::string& fallback_path = "");

    // ---- Phase 2 ----
    //
    // Tulis file yang status != Blocked ke fs. Tidak parse ulang — pakai
    // hasil preview() terakhir. Return ImportResult yang sama dengan
    // preview ditambah field `written_count` dan `failed_count`.
    //
    // Kalau belum pernah preview() atau preview terakhir gagal:
    //   → return ImportResult{parse_failed=true, parse_error="..."}
    //
    // Setelah commit, `has_preview_` di-reset supaya commit kedua tidak
    // menulis ulang. Snapshot tetap tersedia lewat snapshots().
    ImportResult commit();

    // Buang state preview + snapshot. UI panggil setelah dialog close.
    // Idempotent — boleh dipanggil kapan saja.
    void reset();

    // ---- Observability ----

    bool hasPreview() const { return has_preview_; }

    // Snapshot isi file SEBELUM ditulis (path -> content). Entry kosong
    // berarti file baru (belum ada saat snapshot). Populated setelah
    // commit. Tetap tersedia setelah commit — dipakai UI untuk diff.
    const std::map<std::string, std::string>& snapshots() const {
        return snapshots_;
    }

private:
    // Deteksi bahasa dari ekstensi untuk DangerCheckHandler.
    // Return "" kalau ekstensi tidak dikenali (handler akan return Safe).
    static std::string languageFromPath(const std::string& path);

    frans::domain::IFileSystemPort& fs_;
    InvalidateFn                    on_written_;
    DangerCheckHandler              danger_;

    ImportResult                       last_preview_;
    std::map<std::string, std::string> snapshots_;
    bool                               has_preview_ = false;
};

} // namespace frans::application
