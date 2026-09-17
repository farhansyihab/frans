#pragma once

#include <string>

namespace frans::domain {

// Abstraksi minimal untuk operasi file yang dipakai ManualImporter + BuildRunner.
// Implementasi produksi adalah SandboxedFileSystem.
//
// Kontrak:
//   - TIDAK boleh throw. Kegagalan dikomunikasikan lewat return value.
//   - "path" boleh relative (di-resolve terhadap root) atau absolute
//     (harus tetap berada di dalam root, kalau tidak -> tolak).
class IFileSystemPort {
public:
    virtual ~IFileSystemPort() = default;

    virtual bool writeFile(const std::string& path,
                           const std::string& content) = 0;

    virtual std::string readFile(const std::string& path) = 0;

    virtual bool createDirectory(const std::string& path) = 0;

    virtual bool exists(const std::string& path) = 0;

    virtual bool deleteFile(const std::string& path) = 0;

    // Query containment. Default true (mock in-memory FS tidak perlu override).
    // SandboxedFileSystem override untuk cek containment.
    virtual bool isPathSafe(const std::string& path) const {
        (void)path;
        return true;
    }
};

} // namespace frans::domain