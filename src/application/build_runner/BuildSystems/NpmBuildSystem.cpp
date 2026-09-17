// ~/AgenAI/frans/src/application/build_runner/BuildSystems/NpmBuildSystem.cpp
#include "application/build_runner/BuildSystems/NpmBuildSystem.h"

#include <regex>
#include <string>

namespace frans::application {

namespace {

int parseIntSafe(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}

} // namespace

std::vector<PhaseCommand> NpmBuildSystem::buildCommands(
    const ProjectInfo& info,
    const std::vector<GeneratedFile>& /*files*/,
    frans::domain::IFileSystemPort& fs) const
{
    // Baca package.json untuk cek scripts.build.
    const std::string rel = "package.json";
    // Coba relative ke project_root dulu; kalau gagal, coba via
    // relative path relatif terhadap sandbox_root + strip prefix.
    std::string txt = fs.readFile(rel);

    // Kalau sandbox_root != project_root, project_root adalah subfolder.
    // Path relatif terhadap sandbox_root adalah project_root-nya.
    if (txt.empty() && info.project_root != info.sandbox_root
        && !info.sandbox_root.empty())
    {
        std::string prefix = info.sandbox_root;
        if (prefix.back() != '/') prefix += '/';
        if (info.project_root.size() > prefix.size()
            && info.project_root.rfind(prefix, 0) == 0)
        {
            const std::string sub = info.project_root.substr(prefix.size());
            txt = fs.readFile(sub + "/package.json");
        }
    }

    if (txt.find("\"build\"") == std::string::npos) {
        return {};   // tidak ada scripts.build
    }

    return {{ "npm-build", {"npm", "run", "build"} }};
}

std::vector<PhaseCommand> NpmBuildSystem::testCommands(
    const ProjectInfo& info,
    frans::domain::IFileSystemPort& /*fs*/) const
{
    if (info.test_runner != "NpmTest") return {};
    return {{ "npm-test", {"npm", "test", "--silent"} }};
}

std::vector<PhaseCommand> NpmBuildSystem::runCommands(
    const ProjectInfo& /*info*/,
    const std::vector<GeneratedFile>& /*files*/) const
{
    return {};
}

void NpmBuildSystem::parseOutput(
    PhaseResult& result,
    const ProjectInfo& info) const
{
    if (result.success) return;

    // Format TypeScript: `path(line,col): error TSxxxx: message`
    static const std::regex reTs(
        R"re(([^(]+)\((\d+),(\d+)\):\s+(error|warning)\s+TS\d+:\s+([^\n]+))re");

    for (const auto& cmd : result.commands) {
        const std::string& src = !cmd.stdout_tail.empty()
                               ? cmd.stdout_tail
                               : cmd.stderr_tail;

        for (auto it = std::sregex_iterator(src.begin(), src.end(), reTs);
             it != std::sregex_iterator(); ++it)
        {
            const auto& m = *it;
            if (m[4].str() != "error") continue;
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
    result.error_message = "npm command failed.";
}

} // namespace frans::application