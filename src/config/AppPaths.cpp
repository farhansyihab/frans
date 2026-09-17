#include "config/AppPaths.h"

#include <cstdio>
#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <limits.h>
#  include <unistd.h>
#endif

namespace frans::config {

namespace fs = std::filesystem;

std::string executableDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    fs::path p(buf);
    return p.parent_path().string();
#else
    char buf[PATH_MAX];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    fs::path p(buf);
    return p.parent_path().string();
#endif
}

namespace {

bool isReadableFile(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec) || ec) return false;
    if (!fs::is_regular_file(p, ec) || ec) return false;
    return true;
}

} // namespace

std::string findConfigPath(const std::string& argv0) {
    std::vector<fs::path> candidates;
    std::error_code ec;

    // 1. CWD/config/app.json (perilaku terminal lama).
    const fs::path cwd = fs::current_path(ec);
    if (!ec && !cwd.empty()) {
        candidates.push_back(cwd / "config" / "app.json");
    }

    // 2. Tentukan direktori binary.
    std::string exe_dir = executableDir();
    if (exe_dir.empty() && !argv0.empty()) {
        fs::path p(argv0);
        if (p.is_absolute()) {
            exe_dir = p.parent_path().string();
        } else {
            const fs::path abs = fs::absolute(p, ec);
            if (!ec && !abs.empty()) {
                exe_dir = abs.parent_path().string();
            }
        }
    }

    if (!exe_dir.empty()) {
        const fs::path ed(exe_dir);

        // 2a. <exe_dir>/config/app.json
        candidates.push_back(ed / "config" / "app.json");

        // 2b. <exe_dir>/../share/frans/app.json (install layout Unix)
        candidates.push_back(ed / ".." / "share" / "frans" / "app.json");

        // 2c. Walk-up dari exe_dir, max 6 level.
        fs::path cur = ed;
        for (int i = 0; i < 6; ++i) {
            if (cur.empty()) break;
            const fs::path parent = cur.parent_path();
            if (parent.empty() || parent == cur) break;
            candidates.push_back(parent / "config" / "app.json");
            cur = parent;
        }
    }

    for (const auto& c : candidates) {
        if (isReadableFile(c)) {
            std::fprintf(stderr,
                "[frans] config: loaded from %s\n", c.string().c_str());
            std::fflush(stderr);
            return c.string();
        }
    }

    std::fprintf(stderr,
        "[frans] config: tidak ditemukan di %zu lokasi kandidat — "
        "memakai default bawaan.\n",
        candidates.size());
    std::fflush(stderr);
    return {};
}

} // namespace frans::config