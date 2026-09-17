// ~/AgenAI/frans/src/application/build_runner/BuildSystems/MakeBuildSystem.cpp
#include "application/build_runner/BuildSystems/MakeBuildSystem.h"

#include <regex>
#include <string>
#include <thread>

namespace frans::application {

namespace {

int hardwareThreads() {
    const unsigned n = std::thread::hardware_concurrency();
    return n > 0 ? static_cast<int>(n) : 2;
}

int parseIntSafe(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}

} // namespace

std::vector<PhaseCommand> MakeBuildSystem::buildCommands(
    const ProjectInfo& /*info*/,
    const std::vector<GeneratedFile>& /*files*/,
    frans::domain::IFileSystemPort& /*fs*/) const
{
    return {{
        "make",
        {"make", "-j", std::to_string(hardwareThreads())}
    }};
}

std::vector<PhaseCommand> MakeBuildSystem::testCommands(
    const ProjectInfo& info,
    frans::domain::IFileSystemPort& /*fs*/) const
{
    if (info.test_runner != "MakeTest") return {};
    return {{ "make-test", {"make", "test"} }};
}

std::vector<PhaseCommand> MakeBuildSystem::runCommands(
    const ProjectInfo& /*info*/,
    const std::vector<GeneratedFile>& /*files*/) const
{
    return {};
}

void MakeBuildSystem::parseOutput(
    PhaseResult& result,
    const ProjectInfo& info) const
{
    if (result.success) return;

    static const std::regex reErr(
        R"re(([^:\n]+):(\d+):(\d+):\s+(error|fatal error):\s+([^\n]+))re");

    for (const auto& cmd : result.commands) {
        const std::string& src = !cmd.stderr_tail.empty()
                               ? cmd.stderr_tail
                               : cmd.stdout_tail;

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
                               ? it->stderr_tail
                               : it->stdout_tail;
        const std::string last = lastNonEmptyLine(src);
        if (!last.empty()) {
            result.error_message = last;
            return;
        }
    }
    result.error_message = "make failed.";
}

} // namespace frans::application