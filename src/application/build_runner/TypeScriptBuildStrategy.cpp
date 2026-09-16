// ~/AgenAI/frans/src/application/build_runner/TypeScriptBuildStrategy.cpp
#include "application/build_runner/TypeScriptBuildStrategy.h"

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace frans::application {

namespace {

bool hasSuffix(const std::string& s, const char* suffix) {
    const std::size_t n = std::char_traits<char>::length(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool isTsSource(const std::string& p) {
    if (hasSuffix(p, ".d.ts")) return false;
    return hasSuffix(p, ".ts") || hasSuffix(p, ".tsx");
}

bool looksLikeEntry(const std::string& content) {
    return content.find("main(")          != std::string::npos
        || content.find("main (")         != std::string::npos
        || content.find("console.log")    != std::string::npos;
}

std::string stripSandboxPrefix(const std::string& raw,
                                const std::string& root) {
    if (root.empty() || raw.empty()) return raw;
    std::string prefix = root;
    if (prefix.back() != '/') prefix += '/';
    if (raw.size() > prefix.size() && raw.rfind(prefix, 0) == 0) {
        return raw.substr(prefix.size());
    }
    return raw;
}

int parseIntSafe(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}

std::string tailString(const std::string& s, std::size_t max_chars) {
    if (s.size() <= max_chars) return s;
    return std::string("...(truncated)...\n")
         + s.substr(s.size() - max_chars);
}

} // namespace

std::string TypeScriptBuildStrategy::languageName() const {
    return "TypeScript";
}

std::string TypeScriptBuildStrategy::detectEntryPoint(
    const std::vector<GeneratedFile>& files,
    frans::domain::IFileSystemPort& /*fs*/,
    const std::string& /*sandbox_root*/)
{
    std::vector<std::string> ts_files;
    ts_files.reserve(files.size());

    for (const auto& f : files) {
        if (isTsSource(f.relative_path)) {
            ts_files.push_back(f.relative_path);
        }
    }
    if (ts_files.empty()) return {};

    for (const auto& f : files) {
        if (!isTsSource(f.relative_path)) continue;
        if (looksLikeEntry(f.content)) return f.relative_path;
    }

    std::sort(ts_files.begin(), ts_files.end());
    return ts_files.front();
}

std::vector<std::string> TypeScriptBuildStrategy::buildCommand(
    const std::string& entry_point,
    const std::vector<GeneratedFile>& /*files*/,
    const std::string& /*sandbox_root*/)
{
    if (entry_point.empty()) return {};
    return {"npx", "--no-install", "tsc", "--noEmit", entry_point};
}

BuildReport TypeScriptBuildStrategy::parseOutput(
    int exit_code,
    bool timed_out,
    const std::string& stdout_text,
    const std::string& stderr_text,
    double duration_sec,
    const std::string& sandbox_root)
{
    BuildReport r;
    r.language     = "TypeScript";
    r.exit_code    = exit_code;
    r.timed_out    = timed_out;
    r.duration_sec = duration_sec;
    r.stdout_tail  = tailString(stdout_text, 4096);
    r.stderr_tail  = tailString(stderr_text, 4096);

    if (timed_out) {
        r.success = false;
        r.error_message = "Timeout — tsc tidak selesai dalam batas waktu.";
        return r;
    }

    if (exit_code == 0) {
        r.success = true;
        return r;
    }

    if (exit_code == 127) {
        r.success = false;
        r.error_message =
            "tsc tidak ditemukan. Install: npm install -g typescript  "
            "(atau `npm install --save-dev typescript` di root project).";
        return r;
    }

    r.success = false;

    const std::string& src =
        !stdout_text.empty() ? stdout_text : stderr_text;

    static const std::regex re(
        R"re(([^(]+)\((\d+),(\d+)\):\s+(error|warning)\s+TS\d+:\s+([^\n]+))re");

    std::string primary_path;
    int         primary_line = 0;
    std::string primary_msg;

    for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
         it != std::sregex_iterator(); ++it)
    {
        const auto& m = *it;
        const std::string sev = m[4].str();
        if (sev != "error") continue;

        primary_path = stripSandboxPrefix(m[1].str(), sandbox_root);
        primary_line = parseIntSafe(m[2].str(), 0);
        primary_msg  = m[5].str();
        break;
    }

    if (!primary_path.empty()) {
        RelatedFile rf;
        rf.path = primary_path;
        rf.line = primary_line;
        rf.role = "PRIMARY_ERROR";
        rf.note = primary_msg;
        r.related_files.push_back(std::move(rf));
        r.error_message = primary_msg;
    } else {
        r.error_message = "tsc exited with code "
                        + std::to_string(exit_code);
    }

    return r;
}

} // namespace frans::application