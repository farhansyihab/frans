// ~/AgenAI/frans/src/infrastructure/fs/SandboxedFileSystem.h
#pragma once

#include "domain/fs/IFileSystemPort.h"

#include <filesystem>
#include <string>

namespace frans::infrastructure {

// Blueprint §5.8 — Safety Layer.
//
// Implementasi IFileSystemPort dengan satu root sandbox. SETIAP operasi
// memvalidasi bahwa path yang di-resolve masih berada di dalam root:
//   - Path relative di-join ke root.
//   - Path absolute dipakai apa adanya.
//   - Symlink diselesaikan via std::filesystem::weakly_canonical, lalu
//     dicek prefix-nya terhadap root yang juga sudah canonical.
//   - Path yang mengarah ke luar root (via "../" atau symlink) DITOLAK
//     dengan return false + log ke stderr. Tidak silent.
//
// Root sandbox dibuat otomatis di constructor (create_directories, nested).
// Kalau pembuatan gagal (mis. permission), operasi selanjutnya akan gagal
// secara natural; constructor tetap tidak throw.
//
// RAII-friendly: tidak ada state yang perlu dibersihkan; destructor default.
// Non-copyable (semantik kepemilikan root jelas).
class SandboxedFileSystem : public frans::domain::IFileSystemPort {
public:
    // `root_path` boleh relative atau absolute. Disimpan dalam bentuk
    // canonical (symlink resolved) setelah directory-nya dibuat.
    explicit SandboxedFileSystem(std::string root_path);

    SandboxedFileSystem(const SandboxedFileSystem&)            = delete;
    SandboxedFileSystem& operator=(const SandboxedFileSystem&) = delete;

    // ---- IFileSystemPort ----
    bool        writeFile(const std::string& path,
                          const std::string& content) override;
    std::string readFile(const std::string& path) override;
    bool        createDirectory(const std::string& path) override;
    bool        exists(const std::string& path) override;
    bool        deleteFile(const std::string& path) override;

    // Stage 11a — override query containment. Return true kalau
    // resolveSafe() berhasil (path di dalam root).
    bool        isPathSafe(const std::string& path) const override;

    // Akses ke root canonical (untuk logging / test).
    const std::filesystem::path& root() const { return canonical_root_; }

private:
    // Resolve `path` menjadi absolute+canonical dan verifikasi masih
    // di dalam canonical_root_. Return empty path kalau ditolak.
    std::filesystem::path resolveSafe(const std::string& path) const;

    std::filesystem::path canonical_root_;
};

} // namespace frans::infrastructure
