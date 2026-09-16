// ~/AgenAI/frans/src/application/build_runner/PythonBuildStrategy.cpp
#include "application/build_runner/PythonBuildStrategy.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace frans::application {

namespace {

bool hasMainGuard(const std::string& content) {
    return content.find("__name__") != std::string::npos
        && content.find("__main__") != std::string::npos;
}

bool hasSuffix(const std::string& s, const char* suffix) {
    const std::size_t n = std::char_traits<char>::length(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
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
    try {
        return std::stoi(s);
    } catch (...) {
        return def;
    }
}

std::string tailString(const std::string& s, std::size_t max_chars) {
    if (s.size() <= max_chars) return s;
    return std::string("...(truncated)...\n")
         + s.substr(s.size() - max_chars);
}

std::string extractExceptionMessage(const std::string& text) {
    std::istringstream iss(text);
    std::string line;
    std::string last_msg;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (line[0] == ' ' || line[0] == '\t') continue;

        const auto colon = line.find(':');
        if (colon == std::string::npos || colon == 0) continue;

        const std::string head = line.substr(0, colon);
        bool ok = true;
        for (char c : head) {
            if (!std::isalnum(static_cast<unsigned char>(c))
                && c != '_' && c != '.') {
                ok = false;
                break;
            }
        }
        if (!ok) continue;

        const bool looks_like_exc =
            head.find("Error")           != std::string::npos
         || head.find("Exception")       != std::string::npos
         || head.find("Warning")         != std::string::npos
         || head == "KeyboardInterrupt"
         || head == "SystemExit"
         || head == "StopIteration"
         || head == "GeneratorExit";

        if (looks_like_exc) {
            last_msg = line;
        }
    }
    return last_msg;
}

} // namespace

std::string PythonBuildStrategy::languageName() const {
    return "Python";
}

std::string PythonBuildStrategy::detectEntryPoint(
    const std::vector<GeneratedFile>& files,
    frans::domain::IFileSystemPort& /*fs*/,
    const std::string& /*sandbox_root*/)
{
    std::vector<std::string> py_files;
    py_files.reserve(files.size());

    for (const auto& f : files) {
        if (hasSuffix(f.relative_path, ".py")) {
            py_files.push_back(f.relative_path);
        }
    }
    if (py_files.empty()) return {};

    for (const auto& f : files) {
        if (!hasSuffix(f.relative_path, ".py")) continue;
        if (hasMainGuard(f.content)) return f.relative_path;
    }

    std::sort(py_files.begin(), py_files.end());
    return py_files.front();
}

std::vector<std::string> PythonBuildStrategy::buildCommand(
    const std::string& entry_point,
    const std::vector<GeneratedFile>& /*files*/,
    const std::string& /*sandbox_root*/)
{
    if (entry_point.empty()) return {};
    return {"python3", entry_point};
}

BuildReport PythonBuildStrategy::parseOutput(
    int exit_code,
    bool timed_out,
    const std::string& stdout_text,
    const std::string& stderr_text,
    double duration_sec,
    const std::string& sandbox_root)
{
    BuildReport r;
    r.language     = "Python";
    r.exit_code    = exit_code;
    r.timed_out    = timed_out;
    r.duration_sec = duration_sec;
    r.stdout_tail  = tailString(stdout_text, 4096);
    r.stderr_tail  = tailString(stderr_text, 4096);

    if (timed_out) {
        r.success = false;
        r.error_message = "Timeout — proses Python tidak selesai dalam "
                          "batas waktu (30 detik).";
        return r;
    }

    if (exit_code == 0) {
        r.success = true;
        return r;
    }

    if (exit_code == 127) {
        r.success = false;
        r.error_message = "python3 tidak ditemukan di PATH. "
                          "Install: sudo apt install python3";
        return r;
    }

    r.success = false;

    const std::string& src =
        !stderr_text.empty() ? stderr_text : stdout_text;

    // Raw string dengan delimiter `re` supaya `)"` di pattern tidak
    // men-terminate string lebih awal.
    static const std::regex re(R"re(File "([^"]+)", line (\d+))re");
    std::vector<std::pair<std::string, int>> matches;

    for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
         it != std::sregex_iterator(); ++it)
    {
        const auto& m = *it;
        std::string path = stripSandboxPrefix(m[1].str(), sandbox_root);
        const int   line = parseIntSafe(m[2].str(), 0);
        matches.emplace_back(std::move(path), line);
    }

    if (!matches.empty()) {
        const auto& [path, line] = matches.back();
        RelatedFile rf;
        rf.path = path;
        rf.line = line;
        rf.role = "PRIMARY_ERROR";
        rf.note = "lokasi error utama";
        r.related_files.push_back(std::move(rf));
    }

    r.error_message = extractExceptionMessage(src);

    if (r.error_message.empty()) {
        std::string last_line;
        std::istringstream iss(src);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) last_line = line;
        }
        if (!last_line.empty()) {
            r.error_message = last_line;
        } else {
            r.error_message = "python3 exited with code "
                            + std::to_string(exit_code);
        }
    }

    return r;
}

} // namespace frans::application