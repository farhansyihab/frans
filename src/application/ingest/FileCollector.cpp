// src/application/ingest/FileCollector.cpp
//
// Fase B #46 -- implementasi FileCollector.
#include "application/ingest/FileCollector.h"
#include "infrastructure/system/ProcessExecutor.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace frans::application {
namespace fs = std::filesystem;

namespace {
constexpr const char* kLogPrefix = "[frans][file_collector]";

// Direktori yang selalu di-skip saat walker fallback.
// Cocok untuk proyek C++/Node/Python/Java/IDE umum.
const std::vector<std::string>& defaultExcludeDirs() {
    static const std::vector<std::string> v = {
        ".git", "build", "node_modules", "__pycache__",
        ".venv", "venv", "dist", ".idea", ".vscode", "target"
    };
    return v;
}

// Pola secret (diterapkan ke basename).
// Glob sederhana: hanya '*' dan '?' yang didukung.
const std::vector<std::string>& secretPatterns() {
    static const std::vector<std::string> v = {
        ".env", ".env.*",
        "*.pem", "*.key", "*.p12", "*.pfx",
        "id_rsa", "id_ed25519",
        "*.keystore",
        "credentials.json",
        "secrets.*"
    };
    return v;
}

// ---------------------------------------------------------------------------
// Glob matcher sederhana (hanya '*' dan '?').
// ---------------------------------------------------------------------------
bool matchGlob(const std::string& pattern, const std::string& name) {
    std::size_t pi = 0, ni = 0;
    std::size_t star_p = std::string::npos, star_n = 0;
    while (ni < name.size()) {
        if (pi < pattern.size()
            && (pattern[pi] == '?' || pattern[pi] == name[ni]))
        {
            ++pi; ++ni;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            star_p = pi++;
            star_n = ni;
        } else if (star_p != std::string::npos) {
            pi = star_p + 1;
            ni = ++star_n;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*') ++pi;
    return pi == pattern.size();
}

// Normalisasi path: backslash -> forward slash, strip leading "./".
std::string normalizePath(const std::string& p) {
    std::string out = p;
    for (auto& c : out) if (c == '\\') c = '/';
    while (out.size() >= 2 && out[0] == '.' && out[1] == '/') {
        out.erase(0, 2);
    }
    return out;
}

std::string basenameOf(const std::string& path) {
    const auto pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

// Apakah salah satu komponen path (dipisah '/') sama dengan `name`?
bool hasAnyComponent(const std::string& rel_path, const std::string& name) {
    if (name.empty()) return false;
    std::string_view sv(rel_path);
    std::size_t start = 0;
    while (start < sv.size()) {
        const auto slash = sv.find('/', start);
        const std::string_view comp =
            (slash == std::string_view::npos)
            ? sv.substr(start)
            : sv.substr(start, slash - start);
        if (comp == name) return true;
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }
    return false;
}

bool isSecretBasenameImpl(const std::string& basename) {
    for (const auto& pat : secretPatterns()) {
        if (matchGlob(pat, basename)) return true;
    }
    return false;
}

bool isExcludedPathImpl(const std::string& rel_path,
                        const std::vector<std::string>& extra_excludes)
{
    for (const auto& d : defaultExcludeDirs()) {
        if (hasAnyComponent(rel_path, d)) return true;
    }
    for (const auto& ex : extra_excludes) {
        if (ex.find('*') != std::string::npos
            || ex.find('?') != std::string::npos)
        {
            if (matchGlob(ex, basenameOf(rel_path))) return true;
        } else {
            if (hasAnyComponent(rel_path, ex)) return true;
        }
    }
    return false;
}

// Deteksi binary: cek NUL byte di 8 KB pertama.
bool isBinaryContent(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char buf[8192];
    f.read(buf, sizeof(buf));
    const std::size_t got = static_cast<std::size_t>(f.gcount());
    for (std::size_t i = 0; i < got; ++i) {
        if (buf[i] == '\0') return true;
    }
    return false;
}

// Resolve output_dir ke absolute path (canonical).
// Return string kosong bila output_dir kosong atau gagal di-resolve.
std::string resolveOutputDir(const std::string& output_dir,
                             const std::string& project_root)
{
    if (output_dir.empty()) return {};
    std::error_code ec;
    fs::path p(output_dir);
    if (p.is_relative()) p = fs::path(project_root) / p;
    p = fs::weakly_canonical(p, ec);
    if (ec || p.empty()) return {};
    return p.string();
}

// Apakah `file_abs` berada di dalam `dir_abs`?
// Boundary check: sesudah prefix harus ada '/' supaya
// "/tmp/proj_evil/x" tidak match "/tmp/proj".
bool isInsideDir(const std::string& file_abs, const std::string& dir_abs) {
    if (dir_abs.empty() || file_abs.empty()) return false;
    std::string prefix = dir_abs;
    if (prefix.back() != '/') prefix += '/';
    return file_abs.size() > prefix.size()
        && file_abs.compare(0, prefix.size(), prefix) == 0;
}

// ---------------------------------------------------------------------------
// Jalur git: `git ls-files -z --cached --others --exclude-standard`
// ---------------------------------------------------------------------------
std::vector<std::string> runGitLsFiles(const std::string& root,
                                       std::atomic<bool>* cancel)
{
    std::vector<std::string> out;
    std::vector<std::string> argv = {
        "git", "-C", root, "ls-files", "-z",
        "--cached", "--others", "--exclude-standard"
    };
    auto result = frans::infrastructure::runCommand(
        argv, 30, "", root, cancel);
    if (result.exit_code != 0) {
        std::fprintf(stderr,
            "%s git ls-files failed (exit=%d): %s",
            kLogPrefix, result.exit_code, result.stderr_text.c_str());
        std::fflush(stderr);
        return out;
    }
    const std::string& s = result.stdout_text;
    std::size_t start = 0;
    while (start < s.size()) {
        const auto nl = s.find('\0', start);
        const std::size_t end = (nl == std::string::npos) ? s.size() : nl;
        if (end > start) {
            out.push_back(normalizePath(s.substr(start, end - start)));
        }
        start = (nl == std::string::npos) ? s.size() : nl + 1;
    }
    return out;
}

// Cek apakah `root` berada di dalam git work tree.
bool isInsideGitRepo(const std::string& root, std::atomic<bool>* cancel) {
    std::vector<std::string> argv = {
        "git", "-C", root, "rev-parse", "--is-inside-work-tree"
    };
    auto result = frans::infrastructure::runCommand(
        argv, 10, "", root, cancel);
    return result.exit_code == 0;
}

// ---------------------------------------------------------------------------
// Fallback walker (bukan git repo / git tidak tersedia).
// ---------------------------------------------------------------------------
std::vector<std::string> walkFilesystem(const std::string& root,
                                        const CollectOptions& opt,
                                        std::atomic<bool>* cancel)
{
    std::vector<std::string> out;
    std::error_code ec;
    fs::path root_p = fs::weakly_canonical(root, ec);
    if (ec || root_p.empty()) return out;

    const std::string out_dir_abs = resolveOutputDir(opt.output_dir, root);

    auto iter_opts = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root_p, iter_opts, ec);
    if (ec) return out;

    const fs::recursive_directory_iterator end_it;
    for (; it != end_it; it.increment(ec)) {
        if (ec) {
            std::fprintf(stderr, "%s walker error: %s",
                kLogPrefix, ec.message().c_str());
            std::fflush(stderr);
            ec.clear();
            continue;
        }
        if (cancel && cancel->load()) break;

        const auto& entry = *it;
        const std::string base = entry.path().filename().string();

        // Skip directory yang masuk exclude list (jangan recurse).
        bool skip_dir = false;
        for (const auto& d : defaultExcludeDirs()) {
            if (base == d) { skip_dir = true; break; }
        }
        if (!skip_dir) {
            for (const auto& ex : opt.extra_excludes) {
                if (ex.find('*') == std::string::npos
                    && ex.find('?') == std::string::npos
                    && base == ex)
                {
                    skip_dir = true; break;
                }
            }
        }
        if (skip_dir && entry.is_directory()) {
            it.disable_recursion_pending();
            continue;
        }

        // Skip symlink (file maupun directory).
        if (entry.is_symlink()) {
            if (entry.is_directory()) it.disable_recursion_pending();
            continue;
        }
        if (!entry.is_regular_file()) continue;

        // Output dir exclusion.
        if (!out_dir_abs.empty()
            && isInsideDir(entry.path().string(), out_dir_abs))
        {
            continue;
        }

        std::error_code ec2;
        const fs::path rel_fs = fs::relative(entry.path(), root_p, ec2);
        if (ec2 || rel_fs.empty()) continue;
        const std::string rel = normalizePath(rel_fs.string());
        if (rel.empty() || rel == ".") continue;

        if (isExcludedPathImpl(rel, opt.extra_excludes)) continue;

        out.push_back(rel);
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Helper publik (untuk unit test).
// ---------------------------------------------------------------------------
bool isSecretBasename(const std::string& basename) {
    return isSecretBasenameImpl(basename);
}

bool isExcludedPath(const std::string& rel_path,
                    const std::vector<std::string>& extra_excludes)
{
    return isExcludedPathImpl(rel_path, extra_excludes);
}

// ---------------------------------------------------------------------------
// Entry point.
// ---------------------------------------------------------------------------
CollectResult collectFiles(const std::string& project_root,
                           const CollectOptions& opt,
                           std::atomic<bool>* cancel)
{
    CollectResult r;
    if (project_root.empty()) {
        r.error = "project_root kosong";
        return r;
    }
    {
        std::error_code ec;
        if (!fs::is_directory(project_root, ec) || ec) {
            r.error = "project_root bukan direktori: " + project_root;
            return r;
        }
    }

    // ---- Kumpulkan daftar path relatif ----
    std::vector<std::string> paths = runGitLsFiles(project_root, cancel);
    if (!paths.empty()) {
        r.used_git = true;
    } else {
        // paths kosong bisa berarti: (a) git gagal, (b) repo kosong.
        // Cek apakah ini git repo; kalau bukan, fallback walker.
        if (!isInsideGitRepo(project_root, cancel)) {
            paths = walkFilesystem(project_root, opt, cancel);
            r.used_git = false;
        } else {
            r.used_git = true;   // repo git, tapi memang kosong
        }
    }

    const std::string out_dir_abs = resolveOutputDir(opt.output_dir,
                                                     project_root);

    // ---- Filter + klasifikasi tiap file ----
    r.files.reserve(paths.size());
    for (const auto& rel : paths) {
        if (cancel && cancel->load()) {
            r.error = "dibatalkan";
            r.ok = false;
            return r;
        }
        if (rel.empty()) continue;

        // Output dir exclusion (berlaku juga untuk jalur git).
        if (!out_dir_abs.empty()) {
            std::error_code ec;
            fs::path abs_p = fs::path(project_root) / rel;
            abs_p = fs::weakly_canonical(abs_p, ec);
            if (!ec && isInsideDir(abs_p.string(), out_dir_abs)) continue;
        }

        CollectedFile cf;
        cf.relative_path = rel;

        // Cek fisik: ada, bukan symlink, regular file.
        std::error_code ec;
        fs::path full = fs::path(project_root) / rel;
        if (!fs::exists(full, ec) || ec) continue;
        if (fs::is_symlink(full, ec)) continue;
        if (!fs::is_regular_file(full, ec)) continue;

        cf.size_bytes = static_cast<std::size_t>(fs::file_size(full, ec));
        if (ec) cf.size_bytes = 0;

        // Secret check (basename).
        if (opt.exclude_secrets) {
            const std::string base = basenameOf(rel);
            if (isSecretBasenameImpl(base)) {
                cf.skipped = true;
                cf.skip_reason = "secret";
                r.files.push_back(std::move(cf));
                continue;
            }
        }

        // Extra excludes (sudah dicek di walker, tapi untuk jalur git
        // perlu dicek lagi di sini).
        if (isExcludedPathImpl(rel, opt.extra_excludes)) continue;

        // Ukuran.
        if (cf.size_bytes > opt.max_file_bytes) {
            cf.skipped = true;
            cf.skip_reason = "too_large";
            r.files.push_back(std::move(cf));
            continue;
        }

        // Binary detection.
        if (isBinaryContent(full.string())) {
            cf.is_text = false;
            cf.skipped = true;
            cf.skip_reason = "binary";
            r.files.push_back(std::move(cf));
            continue;
        }

        r.files.push_back(std::move(cf));
    }

    // Urutkan leksikografis supaya output deterministik.
    std::sort(r.files.begin(), r.files.end(),
        [](const CollectedFile& a, const CollectedFile& b) {
            return a.relative_path < b.relative_path;
        });

    r.ok = true;
    std::fprintf(stderr,
        "%s collected %zu file(s) from %s (git=%d)",
        kLogPrefix, r.files.size(), project_root.c_str(),
        r.used_git ? 1 : 0);
    std::fflush(stderr);
    return r;
}

} // namespace frans::application