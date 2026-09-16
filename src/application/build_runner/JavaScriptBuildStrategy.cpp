// ~/AgenAI/frans/src/application/build_runner/JavaScriptBuildStrategy.cpp
#include "application/build_runner/JavaScriptBuildStrategy.h"

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

bool isJsSource(const std::string& p) {
    return hasSuffix(p, ".js")  || hasSuffix(p, ".jsx")
        || hasSuffix(p, ".mjs") || hasSuffix(p, ".cjs");
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

std::string extractLastErrorLine(const std::string& text) {
    std::istringstream iss(text);
    std::string line;
    std::string last;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line[0] == ' ' || line[0] == '\t') continue;

        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string head = line.substr(0, colon);
        if (head.empty()) continue;

        bool ok = true;
        for (char c : head) {
            if (!std::isalnum(static_cast<unsigned char>(c))
                && c != '_' && c != '.') {
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        if (head.find("Error") != std::string::npos) {
            last = line;
        }
    }
    return last;
}

} // namespace

std::string JavaScriptBuildStrategy::languageName() const {
    return "JavaScript";
}

std::string JavaScriptBuildStrategy::detectEntryPoint(
    const std::vector<GeneratedFile>& files,
    frans::domain::IFileSystemPort& /*fs*/,
    const std::string& /*sandbox_root*/)
{
    std::vector<std::string> js_files;
    js_files.reserve(files.size());

    for (const auto& f : files) {
        if (isJsSource(f.relative_path)) {
            js_files.push_back(f.relative_path);
        }
    }
    if (js_files.empty()) return {};

    std::sort(js_files.begin(), js_files.end());
    return js_files.front();
}

std::vector<std::string> JavaScriptBuildStrategy::buildCommand(
    const std::string& entry_point,
    const std::vector<GeneratedFile>& /*files*/,
    const std::string& /*sandbox_root*/)
{
    if (entry_point.empty()) return {};
    return {"node", "--check", entry_point};
}

BuildReport JavaScriptBuildStrategy::parseOutput(
    int exit_code,
    bool timed_out,
    const std::string& stdout_text,
    const std::string& stderr_text,
    double duration_sec,
    const std::string& sandbox_root)
{
    BuildReport r;
    r.language     = "JavaScript";
    r.exit_code    = exit_code;
    r.timed_out    = timed_out;
    r.duration_sec = duration_sec;
    r.stdout_tail  = tailString(stdout_text, 4096);
    r.stderr_tail  = tailString(stderr_text, 4096);

    if (timed_out) {
        r.success = false;
        r.error_message = "Timeout — node --check tidak selesai dalam "
                          "batas waktu.";
        return r;
    }

    if (exit_code == 0) {
        r.success = true;
        return r;
    }

    if (exit_code == 127) {
        r.success = false;
        r.error_message = "node tidak ditemukan di PATH. "
                          "Install: sudo apt install nodejs";
        return r;
    }

    r.success = false;

    const std::string& src =
        !stderr_text.empty() ? stderr_text : stdout_text;

    static const std::regex re(
        R"re(^([^\s:][^:\n]*\.[cm]?js):(\d+)$)re");

    std::string primary_path;
    int         primary_line = 0;

    for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
         it != std::sregex_iterator(); ++it)
    {
        const auto& m = *it;
        primary_path = stripSandboxPrefix(m[1].str(), sandbox_root);
        primary_line = parseIntSafe(m[2].str(), 0);
        break;
    }

    const std::string msg = extractLastErrorLine(src);

    if (!primary_path.empty()) {
        RelatedFile rf;
        rf.path = primary_path;
        rf.line = primary_line;
        rf.role = "PRIMARY_ERROR";
        rf.note = msg.empty() ? "syntax error" : msg;
        r.related_files.push_back(std::move(rf));
    }

    if (!msg.empty()) {
        r.error_message = msg;
    } else {
        r.error_message = "node --check exited with code "
                        + std::to_string(exit_code);
    }

    return r;
}

} // namespace frans::application