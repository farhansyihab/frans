#include "application/build_runner/BuildSystems/MesonBuildSystem.h"

#include <regex>
#include <string>

namespace frans::application {

namespace {
int parseIntSafe(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}
} // namespace

std::vector<PhaseCommand> MesonBuildSystem::buildCommands(
    const ProjectInfo&, const std::vector<GeneratedFile>&,
    frans::domain::IFileSystemPort&) const
{
    std::vector<PhaseCommand> out;
    out.push_back({ "setup",   {"meson", "setup", "build"} });
    out.push_back({ "compile", {"meson", "compile", "-C", "build"} });
    return out;
}

std::vector<PhaseCommand> MesonBuildSystem::testCommands(
    const ProjectInfo& info, frans::domain::IFileSystemPort&) const
{
    if (info.test_runner != "MesonTest") return {};
    return {{ "meson-test",
              {"meson", "test", "-C", "build", "--print-errorlogs"} }};
}

std::vector<PhaseCommand> MesonBuildSystem::runCommands(
    const ProjectInfo&, const std::vector<GeneratedFile>&) const
{
    return {};
}

void MesonBuildSystem::parseOutput(
    PhaseResult& result, const ProjectInfo& info) const
{
    if (result.success) return;

    static const std::regex reErr(
        R"re(([^:\n]+):(\d+):(\d+):\s+(error|fatal error):\s+([^\n]+))re");

    for (const auto& cmd : result.commands) {
        const std::string& src = !cmd.stderr_tail.empty()
                               ? cmd.stderr_tail : cmd.stdout_tail;
        for (auto it = std::sregex_iterator(src.begin(), src.end(), reErr);
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

    for (auto it = result.commands.rbegin();
         it != result.commands.rend(); ++it)
    {
        const std::string& src = !it->stderr_tail.empty()
                               ? it->stderr_tail : it->stdout_tail;
        const std::string last = lastNonEmptyLine(src);
        if (!last.empty()) { result.error_message = last; return; }
    }
    result.error_message = "meson failed.";
}

} // namespace frans::application