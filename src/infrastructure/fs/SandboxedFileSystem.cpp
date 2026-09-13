// ~/AgenAI/frans/src/infrastructure/fs/SandboxedFileSystem.cpp
#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace frans::infrastructure {

namespace fs = std::filesystem;

namespace {

// Log rejection / I/O failure dengan prefix konsisten supaya mudah
// di-grep di terminal: [frans][sandbox] ...
void logReject(const char* op, const std::string& path, const char* reason) {
    std::fprintf(stderr,
        "[frans][sandbox] %s REJECT '%s' (%s)\n",
        op, path.c_str(), reason);
    std::fflush(stderr);
}

void logIoError(const char* op, const std::string& path, const char* what) {
    std::fprintf(stderr,
        "[frans][sandbox] %s FAIL '%s' (%s)\n",
        op, path.c_str(), what);
    std::fflush(stderr);
}

} // namespace

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
SandboxedFileSystem::SandboxedFileSystem(std::string root_path) {
    // Buat root (nested). Kalau sudah ada, tidak error.
    std::error_code ec;
    fs::create_directories(root_path, ec);
    if (ec) {
        std::fprintf(stderr,
            "[frans][sandbox] create_directories('%s') gagal: %s "
            "(operasi selanjutnya kemungkinan akan gagal juga)\n",
            root_path.c_str(), ec.message().c_str());
        std::fflush(stderr);
    }

    // Canonicalize root supaya perbandingan prefix konsisten.
    // weakly_canonical: bagian yang belum ada tetap dianggap ada secara
    // lexical — jadi aman meski create_directories di atas gagal.
    canonical_root_ = fs::weakly_canonical(root_path, ec);
    if (ec || canonical_root_.empty()) {
        // Fallback terakhir: absolute (tanpa resolve symlink).
        canonical_root_ = fs::absolute(root_path, ec);
        if (ec || canonical_root_.empty()) {
            canonical_root_ = root_path;  // biarkan apa adanya
        }
    }

    std::fprintf(stderr,
        "[frans][sandbox] root canonical = %s\n",
        canonical_root_.string().c_str());
    std::fflush(stderr);
}

// ---------------------------------------------------------------------------
// resolveSafe — containment check
// ---------------------------------------------------------------------------
fs::path SandboxedFileSystem::resolveSafe(const std::string& path) const {
    if (path.empty()) return {};

    const fs::path p(path);
    const fs::path target = p.is_absolute()
                          ? p
                          : (canonical_root_ / p);

    std::error_code ec;
    // weakly_canonical: resolve symlink pada bagian yang sudah ada,
    // lalu normalize "..". Bagian yang belum ada dibiarkan apa adanya
    // (penting untuk writeFile ke file baru / createDirectory nested).
    const fs::path resolved = fs::weakly_canonical(target, ec);
    if (ec) return {};

    // Containment check lewat lexically_relative:
    //   resolved == root        -> "."          (ok, tapi tidak berguna)
    //   resolved di bawah root  -> "sub/dir"    (ok)
    //   resolved di atas root   -> "..", "../x" (reject)
    //   beda drive              -> ""           (reject; Linux jarang)
    const fs::path rel = resolved.lexically_relative(canonical_root_);
    if (rel.empty()) return {};

    const std::string r = rel.string();
    if (r == ".." || r.rfind("../", 0) == 0) {
        return {};
    }
    return resolved;
}

// ---------------------------------------------------------------------------
// IFileSystemPort implementation
// ---------------------------------------------------------------------------
bool SandboxedFileSystem::writeFile(const std::string& path,
                                    const std::string& content) {
    const fs::path resolved = resolveSafe(path);
    if (resolved.empty()) {
        logReject("writeFile", path, "path keluar sandbox / tidak valid");
        return false;
    }

    std::ofstream f(resolved, std::ios::binary | std::ios::trunc);
    if (!f) {
        logIoError("writeFile", resolved.string().c_str(), "cannot open");
        return false;
    }
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f) {
        logIoError("writeFile", resolved.string().c_str(), "write error");
        return false;
    }
    return true;
}

std::string SandboxedFileSystem::readFile(const std::string& path) {
    const fs::path resolved = resolveSafe(path);
    if (resolved.empty()) {
        logReject("readFile", path, "path keluar sandbox / tidak valid");
        return {};
    }

    std::ifstream f(resolved, std::ios::binary);
    if (!f) {
        logIoError("readFile", resolved.string().c_str(), "cannot open");
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool SandboxedFileSystem::createDirectory(const std::string& path) {
    const fs::path resolved = resolveSafe(path);
    if (resolved.empty()) {
        logReject("createDirectory", path, "path keluar sandbox / tidak valid");
        return false;
    }

    std::error_code ec;
    fs::create_directories(resolved, ec);
    if (ec) {
        logIoError("createDirectory", resolved.string().c_str(),
                   ec.message().c_str());
        return false;
    }
    return true;
}

bool SandboxedFileSystem::exists(const std::string& path) {
    const fs::path resolved = resolveSafe(path);
    if (resolved.empty()) return false;

    std::error_code ec;
    return fs::exists(resolved, ec);
}

bool SandboxedFileSystem::deleteFile(const std::string& path) {
    const fs::path resolved = resolveSafe(path);
    if (resolved.empty()) {
        logReject("deleteFile", path, "path keluar sandbox / tidak valid");
        return false;
    }

    std::error_code ec;
    // fs::remove menghapus file atau directory kosong. Return value
    // diabaikan karena kita ingin idempotent: file yang tidak ada
    // dianggap "sudah terhapus".
    fs::remove(resolved, ec);
    if (ec) {
        logIoError("deleteFile", resolved.string().c_str(),
                   ec.message().c_str());
        return false;
    }
    return true;
}

} // namespace frans::infrastructure
