#include "application/build_runner/ProjectDetector.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace frans::application {

namespace fs = std::filesystem;

namespace {

constexpr const char* kLogPrefix = "[frans][project_detector]";

struct MarkerSpec {
    BuildSystemKind           kind;
    std::vector<std::string>  filenames;
    std::string               language;
};

const std::vector<MarkerSpec>& markerSpecs() {
    static const std::vector<MarkerSpec> specs = {
        { BuildSystemKind::CMake,  {"CMakeLists.txt"},  "C++"  },
        { BuildSystemKind::Meson,  {"meson.build"},     "C++"  },
        { BuildSystemKind::Make,   {"Makefile", "GNUmakefile", "makefile"}, "C++" },
        { BuildSystemKind::Npm,    {"package.json"},    "JavaScript" },
        { BuildSystemKind::Poetry, {"pyproject.toml"},  "Python" },
    };
    return specs;
}

bool probeDir(const fs::path& dir,
              BuildSystemKind& out_kind,
              std::string& out_marker,
              std::string& out_language)
{
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;

    for (const auto& spec : markerSpecs()) {
        for (const auto& name : spec.filenames) {
            if (fs::exists(dir / name, ec) && !ec) {
                out_kind     = spec.kind;
                out_marker   = name;
                out_language = spec.language;
                return true;
            }
        }
    }
    return false;
}

std::string detectTestRunner(const fs::path& root, BuildSystemKind kind) {
    std::error_code ec;

    if (kind == BuildSystemKind::CMake) {
        std::ifstream f(root / "CMakeLists.txt");
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            const std::string txt = ss.str();
            if (txt.find("enable_testing") != std::string::npos
             || txt.find("add_test")       != std::string::npos
             || txt.find("include(CTest)") != std::string::npos) {
                return "CTest";
            }
        }
        return {};
    }

    if (kind == BuildSystemKind::Make) {
        const char* names[] = {"Makefile", "GNUmakefile", "makefile"};
        for (const char* nm : names) {
            std::ifstream f(root / nm);
            if (!f) continue;
            std::string line;
            while (std::getline(f, line)) {
                if (line.rfind("test:", 0) == 0
                 || line.rfind("test :", 0) == 0
                 || line.rfind(".PHONY: test", 0) == 0) {
                    return "MakeTest";
                }
            }
        }
        return {};
    }

    if (kind == BuildSystemKind::Meson) return "MesonTest";

    if (kind == BuildSystemKind::Npm) {
        std::ifstream f(root / "package.json");
        if (!f) return {};
        std::ostringstream ss;
        ss << f.rdbuf();
        const std::string txt = ss.str();
        if (txt.find("\"test\"") != std::string::npos) return "NpmTest";
        return {};
    }

    if (kind == BuildSystemKind::Poetry) {
        if (fs::exists(root / "pytest.ini", ec)
         || fs::exists(root / "conftest.py", ec)
         || fs::exists(root / "tests", ec)) {
            return "Pytest";
        }
        return {};
    }

    if (fs::is_directory(root, ec)) {
        for (auto it = fs::directory_iterator(root, ec);
             it != fs::directory_iterator(); it.increment(ec))
        {
            if (ec) break;
            const auto name = it->path().filename().string();
            if (name.rfind("test_", 0) == 0
                && it->path().extension() == ".py") {
                return "Pytest";
            }
            if (name.size() > 8
                && name.compare(name.size() - 8, 8, "_test.py") == 0) {
                return "Pytest";
            }
        }
    }
    return {};
}

std::string detectPrimaryLanguage(
    const std::vector<GeneratedFile>& files)
{
    std::map<std::string, int> counts;
    for (const auto& f : files) {
        const auto dot = f.relative_path.find_last_of('.');
        if (dot == std::string::npos) continue;
        const std::string ext = f.relative_path.substr(dot);

        if (ext == ".py" || ext == ".pyw" || ext == ".pyi")      counts["Python"]++;
        else if (ext == ".cpp" || ext == ".cc" || ext == ".cxx"
              || ext == ".hpp" || ext == ".hxx" || ext == ".hh"
              || ext == ".h")                                    counts["C++"]++;
        else if (ext == ".ts" || ext == ".tsx")                  counts["TypeScript"]++;
        else if (ext == ".js" || ext == ".jsx"
              || ext == ".mjs" || ext == ".cjs")                 counts["JavaScript"]++;
        else if (ext == ".vue")                                  counts["Vue"]++;
    }
    if (counts.empty()) return "Unknown";
    return std::max_element(
        counts.begin(), counts.end(),
        [](const auto& a, const auto& b){ return a.second < b.second; }
    )->first;
}

} // namespace

const char* buildSystemKindName(BuildSystemKind k) {
    switch (k) {
        case BuildSystemKind::CMake:      return "CMake";
        case BuildSystemKind::Make:       return "Make";
        case BuildSystemKind::Meson:      return "Meson";
        case BuildSystemKind::Npm:        return "npm";
        case BuildSystemKind::Poetry:     return "Poetry";
        case BuildSystemKind::Standalone: return "Standalone";
    }
    return "?";
}

ProjectInfo ProjectDetector::detect(
    const std::vector<GeneratedFile>& files_written,
    const std::string& sandbox_root)
{
    ProjectInfo info;
    info.sandbox_root = sandbox_root;

    if (sandbox_root.empty()) {
        info.project_root = sandbox_root;
        info.primary_language = detectPrimaryLanguage(files_written);
        return info;
    }

    std::error_code ec;
    fs::path canonical_sandbox = fs::weakly_canonical(sandbox_root, ec);
    if (ec || canonical_sandbox.empty()) canonical_sandbox = sandbox_root;

    std::vector<std::string> candidate_roots;

    for (const auto& f : files_written) {
        if (f.relative_path.empty()) continue;

        fs::path file_abs = canonical_sandbox / f.relative_path;
        file_abs = fs::weakly_canonical(file_abs, ec);
        if (ec) continue;

        info.imported_absolute_paths.push_back(file_abs.string());
        info.imported_relative_paths.push_back(f.relative_path);

        fs::path dir = file_abs.parent_path();
        if (dir.empty()) continue;

        for (fs::path cur = dir; ; cur = cur.parent_path()) {
            BuildSystemKind tmp_kind;
            std::string tmp_marker, tmp_lang;
            if (probeDir(cur, tmp_kind, tmp_marker, tmp_lang)) {
                candidate_roots.push_back(cur.string());
                break;
            }
            if (cur == canonical_sandbox) break;
            if (cur.empty() || cur == cur.parent_path()) break;
        }
    }

    if (candidate_roots.empty()) {
        info.project_root = canonical_sandbox.string();
    } else {
        std::map<std::string, int> votes;
        for (const auto& c : candidate_roots) votes[c]++;

        std::string best;
        int best_votes = 0;
        for (const auto& [path, v] : votes) {
            if (v > best_votes
                || (v == best_votes && path.size() < best.size()))
            {
                best = path;
                best_votes = v;
            }
        }
        info.project_root = best;
    }

    {
        BuildSystemKind kind;
        std::string marker, lang;
        if (probeDir(info.project_root, kind, marker, lang)) {
            info.kind             = kind;
            info.primary_language = lang;   // hint dari marker
            info.markers_found.push_back(marker);
        } else {
            info.kind = BuildSystemKind::Standalone;
            // fallback di bawah (detectPrimaryLanguage)
        }

        // Override language dari marker dengan deteksi aktual dari
        // file yang di-import, KALAU ada file yang bisa dideteksi.
        //
        // Contoh kasus: project npm dengan tsconfig.json + src/*.ts.
        // Marker package.json hardcode language="JavaScript", tapi
        // file aktual jelas TypeScript. Override -> "TypeScript".
        //
        // Marker language tetap dipakai sebagai fallback kalau
        // detectPrimaryLanguage() return "Unknown" (mis. hanya ada
        // file .json / .txt / config).
        const std::string detected = detectPrimaryLanguage(files_written);
        if (detected != "Unknown") {
            info.primary_language = detected;
        }
    }

    info.test_runner = detectTestRunner(info.project_root, info.kind);

    std::fprintf(stderr,
        "%s sandbox=%s project_root=%s kind=%s test=%s lang=%s\n",
        kLogPrefix,
        info.sandbox_root.c_str(),
        info.project_root.c_str(),
        buildSystemKindName(info.kind),
        info.test_runner.empty() ? "(none)" : info.test_runner.c_str(),
        info.primary_language.c_str());
    std::fflush(stderr);

    return info;
}

} // namespace frans::application
