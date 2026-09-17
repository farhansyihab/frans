// ~/AgenAI/frans/src/application/build_runner/BuildSystems/PoetryBuildSystem.cpp
#include "application/build_runner/BuildSystems/PoetryBuildSystem.h"

#include <regex>
#include <string>

namespace frans::application {

namespace {

int parseIntSafe(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}

// Cari file .py yang punya `if __name__ == "__main__":`.
std::string findPythonEntry(
    const std::vector<GeneratedFile>& files)
{
    for (const auto& f : files) {
        const auto dot = f.relative_path.find_last_of('.');
        if (dot == std::string::npos) continue;
        if (f.relative_path.substr(dot) != ".py") continue;
        if (f.content.find("__name__")    != std::string::npos
            && f.content.find("__main__") != std::string::npos) {
            return f.relative_path;
        }
    }
    // Fallback: .py pertama.
    for (const auto& f : files) {
        const auto dot = f.relative_path.find_last_of('.');
        if (dot == std::string::npos) continue;
        if (f.relative_path.substr(dot) == ".py") return f.relative_path;
    }
    return {};
}

} // namespace

std::vector<PhaseCommand> PoetryBuildSystem::buildCommands(
    const ProjectInfo& /*info*/,
    const std::vector<GeneratedFile>& /*files*/,
    frans::domain::IFileSystemPort& /*fs*/) const
{
    // Python tidak butuh build step.
    return {};
}

std::vector<PhaseCommand> PoetryBuildSystem::testCommands(
    const ProjectInfo& info,
    frans::domain::IFileSystemPort& /*fs*/) const
{
    if (info.test_runner != "Pytest") return {};
    return {{
        "pytest",
        {"poetry", "run", "pytest", "-q"}
    }};
}

std::vector<PhaseCommand> PoetryBuildSystem::runCommands(
    const ProjectInfo& info,
    const std::vector<GeneratedFile>& files) const
{
    (void)info;
    const std::string entry = findPythonEntry(files);
    if (entry.empty()) return {};
    return {{
        "run",
        {"poetry", "run", "python", entry}
    }};
}

void PoetryBuildSystem::parseOutput(
    PhaseResult& result,
    const ProjectInfo& info) const
{
    if (result.success) return;

    static const std::regex rePy(
        R"re(File "([^"]+)", line (\d+))re");

    for (const auto& cmd : result.commands) {
        const std::string& src = !cmd.stderr_tail.empty()
                               ? cmd.stderr_tail
                               : cmd.stdout_tail;

        std::smatch m;
        std::string last_match_path;
        int         last_match_line = 0;
        std::string s = src;
        while (std::regex_search(s, m, rePy)) {
            last_match_path = stripRoot(m[1].str(), info.project_root);
            last_match_line = parseIntSafe(m[2].str(), 0);
            s = m.suffix().str();
        }

        if (!last_match_path.empty()) {
            RelatedFile rf;
            rf.path = last_match_path;
            rf.line = last_match_line;
            rf.role = "PRIMARY_ERROR";
            rf.note = "python error";
            result.related_files.push_back(std::move(rf));

            // Extract exception line (baris terakhir non-indented).
            std::string line, last;
            std::size_t p = 0;
            while (p < src.size()) {
                const std::size_t nl = src.find('\n', p);
                line = (nl == std::string::npos) ? src.substr(p)
                                                 : src.substr(p, nl - p);
                p = (nl == std::string::npos) ? src.size() : nl + 1;
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()
                    && line[0] != ' '
                    && line[0] != '\t'
                    && line.find(':') != std::string::npos) {
                    last = line;
                }
            }
            if (!last.empty()) {
                result.error_message = last;
                return;
            }
        }
    }

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
    result.error_message = "Poetry command failed.";
}

} // namespace frans::application