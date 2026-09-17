// ~/AgenAI/frans/src/application/build_runner/BuildSystems/StandaloneBuildSystem.cpp
#include "application/build_runner/BuildSystems/StandaloneBuildSystem.h"

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>

namespace frans::application {

namespace {

bool hasSuffix(const std::string& s, const char* suf) {
    const std::size_t n = std::char_traits<char>::length(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

bool isCppSource(const std::string& p) {
    return hasSuffix(p, ".cpp") || hasSuffix(p, ".cc") || hasSuffix(p, ".cxx");
}

bool hasMainFunction(const std::string& content) {
    return content.find("int main(")  != std::string::npos
        || content.find("int main (") != std::string::npos;
}

int parseIntSafe(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}

} // namespace

std::string StandaloneBuildSystem::findPythonEntry(
    const std::vector<GeneratedFile>& files)
{
    for (const auto& f : files) {
        if (!hasSuffix(f.relative_path, ".py")) continue;
        if (f.content.find("__name__")    != std::string::npos
         && f.content.find("__main__")    != std::string::npos) {
            return f.relative_path;
        }
    }
    std::vector<std::string> py;
    for (const auto& f : files) {
        if (hasSuffix(f.relative_path, ".py")) py.push_back(f.relative_path);
    }
    if (py.empty()) return {};
    std::sort(py.begin(), py.end());
    return py.front();
}

std::string StandaloneBuildSystem::findCppEntry(
    const std::vector<GeneratedFile>& files)
{
    for (const auto& f : files) {
        if (!isCppSource(f.relative_path)) continue;
        if (hasMainFunction(f.content)) return f.relative_path;
    }
    std::vector<std::string> cpp;
    for (const auto& f : files) {
        if (isCppSource(f.relative_path)) cpp.push_back(f.relative_path);
    }
    if (cpp.empty()) return {};
    std::sort(cpp.begin(), cpp.end());
    return cpp.front();
}

std::string StandaloneBuildSystem::findTsEntry(
    const std::vector<GeneratedFile>& files)
{
    for (const auto& f : files) {
        if (!hasSuffix(f.relative_path, ".ts")
            && !hasSuffix(f.relative_path, ".tsx")) continue;
        if (hasSuffix(f.relative_path, ".d.ts")) continue;
        return f.relative_path;
    }
    return {};
}

std::string StandaloneBuildSystem::findJsEntry(
    const std::vector<GeneratedFile>& files)
{
    for (const auto& f : files) {
        if (hasSuffix(f.relative_path, ".js")
            || hasSuffix(f.relative_path, ".mjs")
            || hasSuffix(f.relative_path, ".cjs")) {
            return f.relative_path;
        }
    }
    return {};
}

std::vector<PhaseCommand> StandaloneBuildSystem::buildCommands(
    const ProjectInfo& info,
    const std::vector<GeneratedFile>& files,
    frans::domain::IFileSystemPort& /*fs*/) const
{
    // C++: kumpulkan semua source, compile.
    if (info.primary_language == "C++") {
        std::vector<std::string> sources;
        bool has_main = false;
        for (const auto& f : files) {
            if (!isCppSource(f.relative_path)) continue;
            sources.push_back(f.relative_path);
            if (hasMainFunction(f.content)) has_main = true;
        }
        if (sources.empty()) return {};

        std::vector<std::string> argv;
        argv.push_back("g++");
        argv.push_back("-std=c++20");
        argv.push_back("-I");
        argv.push_back(".");

        if (has_main) {
            argv.push_back("-o");
            argv.push_back("a.out");
            for (const auto& s : sources) argv.push_back(s);
        } else {
            argv.push_back("-fsyntax-only");
            for (const auto& s : sources) argv.push_back(s);
        }
        return {{ has_main ? "compile" : "syntax-check", std::move(argv) }};
    }

    // TypeScript: tsc --noEmit pada entry.
    if (info.primary_language == "TypeScript") {
        const std::string entry = findTsEntry(files);
        if (entry.empty()) return {};
        return {{
            "tsc-check",
            {"npx", "--no-install", "tsc", "--noEmit", entry}
        }};
    }

    // JavaScript: node --check.
    if (info.primary_language == "JavaScript") {
        const std::string entry = findJsEntry(files);
        if (entry.empty()) return {};
        return {{
            "node-check",
            {"node", "--check", entry}
        }};
    }

    // Python: skip build.
    return {};
}

std::vector<PhaseCommand> StandaloneBuildSystem::testCommands(
    const ProjectInfo& info,
    frans::domain::IFileSystemPort& /*fs*/) const
{
    if (info.test_runner != "Pytest") return {};
    // Hanya untuk project Python.
    if (info.primary_language != "Python") return {};
    return {{
        "pytest",
        {"python3", "-m", "pytest", "-q", "--no-header"}
    }};
}

std::vector<PhaseCommand> StandaloneBuildSystem::runCommands(
    const ProjectInfo& info,
    const std::vector<GeneratedFile>& files) const
{
    if (info.primary_language == "Python") {
        const std::string entry = findPythonEntry(files);
        if (entry.empty()) return {};
        return {{ "run", {"python3", entry} }};
    }
    if (info.primary_language == "C++") {
        // Cek apakah ada main.
        for (const auto& f : files) {
            if (isCppSource(f.relative_path)
                && hasMainFunction(f.content)) {
                return {{ "run", {"./a.out"} }};
            }
        }
        return {};
    }
    if (info.primary_language == "JavaScript") {
        const std::string entry = findJsEntry(files);
        if (entry.empty()) return {};
        return {{ "run", {"node", entry} }};
    }
    return {};
}

void StandaloneBuildSystem::parseOutput(
    PhaseResult& result,
    const ProjectInfo& info) const
{
    if (result.success) return;

    // Coba parsing error tergantung bahasa.
    const bool is_cpp = (info.primary_language == "C++");
    const bool is_py  = (info.primary_language == "Python");
    const bool is_js  = (info.primary_language == "JavaScript"
                       || info.primary_language == "TypeScript");

    for (const auto& cmd : result.commands) {
        const std::string& src = !cmd.stderr_tail.empty()
                               ? cmd.stderr_tail
                               : cmd.stdout_tail;
        if (src.empty()) continue;

        if (is_cpp) {
            static const std::regex re(
                R"re(([^:\n]+):(\d+):(\d+):\s+(fatal error|error):\s+([^\n]+))re");
            for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
                 it != std::sregex_iterator(); ++it)
            {
                const auto& m = *it;
                RelatedFile rf;
                rf.path = stripRoot(m[1].str(), info.project_root);
                rf.line = parseIntSafe(m[2].str(), 0);
                rf.role = "PRIMARY_ERROR";
                rf.note = m[5].str();
                result.related_files.push_back(std::move(rf));
                result.error_message = m[5].str();
                return;
            }
        }

        if (is_py) {
            static const std::regex re(R"re(File "([^"]+)", line (\d+))re");
            std::smatch m;
            std::string last_path;
            int         last_line = 0;
            std::string s = src;
            while (std::regex_search(s, m, re)) {
                last_path = stripRoot(m[1].str(), info.project_root);
                last_line = parseIntSafe(m[2].str(), 0);
                s = m.suffix().str();
            }
            if (!last_path.empty()) {
                RelatedFile rf;
                rf.path = last_path;
                rf.line = last_line;
                rf.role = "PRIMARY_ERROR";
                rf.note = "python error";
                result.related_files.push_back(std::move(rf));
            }
        }

        if (is_js) {
            static const std::regex re(
                R"re(([^\s:][^:\n]*\.[cm]?[jt]sx?):(\d+))re");
            for (auto it = std::sregex_iterator(src.begin(), src.end(), re);
                 it != std::sregex_iterator(); ++it)
            {
                const auto& m = *it;
                RelatedFile rf;
                rf.path = stripRoot(m[1].str(), info.project_root);
                rf.line = parseIntSafe(m[2].str(), 0);
                rf.role = "PRIMARY_ERROR";
                rf.note = "syntax error";
                result.related_files.push_back(std::move(rf));
                break;
            }
        }
    }

    // Fallback: last non-empty line.
    for (auto it = result.commands.rbegin();
         it != result.commands.rend(); ++it)
    {
        const std::string& src = !it->stderr_tail.empty()
                               ? it->stderr_tail
                               : it->stdout_tail;
        const std::string last = lastNonEmptyLine(src);
        if (!last.empty()) {
            result.error_message = last;
            return;
        }
    }
    result.error_message = "Standalone build failed.";
}

} // namespace frans::application