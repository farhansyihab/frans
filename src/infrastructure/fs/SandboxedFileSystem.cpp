#include "infrastructure/fs/SandboxedFileSystem.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

namespace frans::infrastructure {

namespace fs = std::filesystem;

namespace {

void logReject(const char* op, const std::string& path, const char* reason) {
    std::fprintf(stderr, "[frans][sandbox] %s REJECT '%s' (%s)\n",
        op, path.c_str(), reason);
    std::fflush(stderr);
}

void logIoError(const char* op, const std::string& path, const char* what) {
    std::fprintf(stderr, "[frans][sandbox] %s FAIL '%s' (%s)\n",
        op, path.c_str(), what);
    std::fflush(stderr);
}

// Case-insensitive compare untuk Windows (drive letter & path).
// Di POSIX, ini no-op (case-sensitive).
bool pathEquals(const fs::path& a, const fs::path& b) {
#ifdef _WIN32
    std::string sa = a.generic_string();
    std::string sb = b.generic_string();
    for (auto& c : sa) c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
    for (auto& c : sb) c = static_cast<char>(
        std::tolower(static_cast<unsigned char>(c)));
    return sa == sb;
#else
    return a == b;
#endif
}

// Cek apakah `path` berada di luar `root` berdasarkan representasi
// relatif. Return true kalau escape terdeteksi (relatif diawali '..').
//
// Catatan: pakai `generic_string()` supaya separator selalu '/',
// jadi tidak peduli OS. Ini fix untuk Windows di mana `rel.string()`
// mengembalikan '\' (backslash).
bool isEscapingRelative(const fs::path& rel) {
    if (rel.empty()) return true;  // tidak bisa ditentukan -> treat as escape

    const std::string r = rel.generic_string();
    if (r == "..") return true;
    if (r.rfind("../", 0) == 0) return true;
    return false;
}

} // namespace

SandboxedFileSystem::SandboxedFileSystem(std::string root_path) {
    std::error_code ec;
    fs::create_directories(root_path, ec);
    if (ec) {
        std::fprintf(stderr,
            "[frans][sandbox] create_directories('%s') gagal: %s\n",
            root_path.c_str(), ec.message().c_str());
        std::fflush(stderr);
    }

    canonical_root_ = fs::weakly_canonical(root_path, ec);
    if (ec || canonical_root_.empty()) {
        canonical_root_ = fs::absolute(root_path, ec);
        if (ec || canonical_root_.empty()) canonical_root_ = root_path;
    }

    std::fprintf(stderr, "[frans][sandbox] root canonical = %s\n",
        canonical_root_.string().c_str());
    std::fflush(stderr);
}

fs::path SandboxedFileSystem::resolveSafe(const std::string& path) const {
    if (path.empty()) return {};

    const fs::path p(path);
    const fs::path target = p.is_absolute() ? p : (canonical_root_ / p);

    std::error_code ec;
    const fs::path resolved = fs::weakly_canonical(target, ec);
    if (ec) return {};

    // Quick reject: kalau resolve menghasilkan path yang SAMA dengan root
    // (mis. input "" atau "."), itu bukan file yang valid.
    // (Bukan escape, tapi jangan lolos ke writeFile.)

    const fs::path rel = resolved.lexically_relative(canonical_root_);
    if (rel.empty()) return {};

    // FIX: pakai generic_string() supaya separator selalu '/',
    // cross-platform. Sebelumnya pakai `rel.string()` yang di
    // Windows menghasilkan '..\evil.py' sehingga cek `../` gagal.
    if (isEscapingRelative(rel)) return {};

    // Extra safety: cek apakah resolved benar-benar di bawah root
    // secara case-insensitive di Windows. Ini melindungi dari
    // edge case di mana lexically_relative gagal karena case
    // mismatch (mis. C:\Users vs c:\users).
    {
        const std::string resolved_str = resolved.generic_string();
        const std::string root_str     = canonical_root_.generic_string();

        if (resolved_str.size() <= root_str.size()) return {};

#ifdef _WIN32
        // Case-insensitive prefix check di Windows.
        std::string a = resolved_str.substr(0, root_str.size());
        std::string b = root_str;
        for (auto& c : a) c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
        for (auto& c : b) c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
        if (a != b) return {};
#else
        if (resolved_str.compare(0, root_str.size(), root_str) != 0) return {};
#endif

        // Pastikan karakter setelah root adalah separator, supaya
        // "/tmp/proj_evil" tidak match "/tmp/proj".
        const char after = resolved_str[root_str.size()];
        if (after != '/' && after != '\\') return {};
    }

    return resolved;
}

bool SandboxedFileSystem::isPathSafe(const std::string& path) const {
    return !resolveSafe(path).empty();
}

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
    fs::remove(resolved, ec);
    if (ec) {
        logIoError("deleteFile", resolved.string().c_str(),
                   ec.message().c_str());
        return false;
    }
    return true;
}

} // namespace frans::infrastructure