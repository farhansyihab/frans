#include "application/build_runner/BuildSystems/CMakeBuildSystem.h"

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

std::vector<PhaseCommand> CMakeBuildSystem::buildCommands(
    const ProjectInfo&, const std::vector<GeneratedFile>&,
    frans::domain::IFileSystemPort&) const
{
    const int n = hardwareThreads();
    std::vector<PhaseCommand> out;
    out.push_back({
        "configure",
        {"cmake", "-S", ".", "-B", "build",
         "-DCMAKE_BUILD_TYPE=Release"}
    });
    out.push_back({
        "compile",
        {"cmake", "--build", "build", "-j", std::to_string(n)}
    });
    return out;
}

std::vector<PhaseCommand> CMakeBuildSystem::testCommands(
    const ProjectInfo& info, frans::domain::IFileSystemPort&) const
{
    if (info.test_runner != "CTest") return {};
    return {{ "ctest",
              {"ctest", "--test-dir", "build", "--output-on-failure"} }};
}

std::vector<PhaseCommand> CMakeBuildSystem::runCommands(
    const ProjectInfo&, const std::vector<GeneratedFile>&) const
{
    return {};
}

void CMakeBuildSystem::parseOutput(
    PhaseResult& result, const ProjectInfo& info) const
{
    if (result.success) return;

    static const std::regex reErr(
        R"re(([^:\n]+):(\d+):(\d+):\s+(fatal error|error):\s+([^\n]+))re");
    static const std::regex reCmakeErr(
        R"re(CMake Error at ([^:\n]+):(\d+)[^\n]*:\n\s*([^\n]+))re");

    for (const auto& cmd : result.commands) {
        const std::string& src = !cmd.stderr_tail.empty()
                               ? cmd.stderr_tail : cmd.stdout_tail;
        if (src.empty()) continue;

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

        std::smatch m;
        if (std::regex_search(src, m, reCmakeErr)) {
            RelatedFile rf;
            rf.path = stripRoot(m[1].str(), info.project_root);
            rf.line = parseIntSafe(m[2].str(), 0);
            rf.role = "PRIMARY_ERROR";
            rf.note = m[3].str();
            result.related_files.push_back(std::move(rf));
            result.error_message = m[3].str();
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
    result.error_message = "CMake build failed.";
}

} // namespace frans::application