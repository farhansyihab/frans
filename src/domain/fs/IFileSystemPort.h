// ~/AgenAI/frans/src/domain/fs/IFileSystemPort.h
#pragma once

#include <string>

namespace frans::domain {

// Blueprint §4 / §5.8 — FileSystemPort.
//
// Abstraksi minimal untuk operasi file yang dipakai Agent di Stage 7b-7d.
// Implementasi produksi adalah SandboxedFileSystem (infrastructure layer),
// yang WAJIB menolak path keluar dari root sandbox.
//
// Kontrak:
//   - TIDAK boleh throw. Semua kegagalan dikomunikasikan lewat return value.
//   - "path" boleh relative (di-resolve terhadap root) atau absolute
//     (harus tetap berada di dalam root, kalau tidak → tolak).
//   - readFile() mengembalikan std::string kosong untuk file kosong ATAU
//     file yang tidak bisa dibaca. Kalau caller butuh membedakan, pakai
//     exists() dulu.
class IFileSystemPort {
public:
    virtual ~IFileSystemPort() = default;

    // Tulis `content` ke `path` (truncate kalau sudah ada).
    // Parent directory TIDAK dibuat otomatis — panggil createDirectory() dulu.
    // Return false kalau path ditolak sandbox, atau ada error I/O.
    virtual bool writeFile(const std::string& path,
                           const std::string& content) = 0;

    // Baca seluruh isi file. Return "" kalau gagal / tidak ada.
    virtual std::string readFile(const std::string& path) = 0;

    // Buat directory (nested, seperti `mkdir -p`). Return true kalau
    // directory ada pada saat return (baik baru dibuat atau sudah ada).
    virtual bool createDirectory(const std::string& path) = 0;

    // Cek keberadaan file atau directory.
    virtual bool exists(const std::string& path) = 0;

    // Hapus file (atau directory kosong). Idempotent — return true kalau
    // setelah return path sudah tidak ada, false kalau ditolak sandbox
    // atau ada error I/O.
    virtual bool deleteFile(const std::string& path) = 0;
};

} // namespace frans::domain
