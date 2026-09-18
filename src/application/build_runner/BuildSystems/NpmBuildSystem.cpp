#include "application/build_runner/BuildSystems/NpmBuildSystem.h"

#include <regex>
#include <string>
#include <vector>

namespace frans::application {

namespace {
int parseIntSafe(const std::string& s, int def = 0) {
    try { return std::stoi(s); } catch (...) { return def; }
}
} // namespace

std::string NpmBuildSystem::relProjectToSandbox(const ProjectInfo& info) {
    // Return path relatif project_root terhadap sandbox_root.
    //   ""      -> project_root == sandbox_root
    //   "sub"   -> project_root = sandbox_root/sub
    //   {}      -> di luar sandbox (tidak valid untuk akses fs)
    if (info.sandbox_root.empty()) return {};
    if (info.project_root == info.sandbox_root) return {};

    std::string prefix = info.sandbox_root;
    if (prefix.back() != '/') prefix += '/';

    if (info.project_root.size() <= prefix.size()) return {};
    if (info.project_root.rfind(prefix, 0) != 0) return {};

    return info.project_root.substr(prefix.size());
}

bool NpmBuildSystem::packageJsonHasField(const std::string& json,
                                         const std::string& field) {
    const std::string needle = "\"" + field + "\"";
    return json.find(needle) != std::string::npos;
}

std::vector<PhaseCommand> NpmBuildSystem::buildCommands(
    const ProjectInfo& info, const std::vector<GeneratedFile>&,
    frans::domain::IFileSystemPort& fs) const
{
    const std::string rel = relProjectToSandbox(info);
    const bool at_root    = rel.empty();

    const std::string pkg_path = at_root ? "package.json"
                                         : (rel + "/package.json");
    const std::string nm_path  = at_root ? "node_modules"
                                         : (rel + "/node_modules");

    const std::string txt = fs.readFile(pkg_path);
    if (txt.empty()) return {};

    std::vector<PhaseCommand> out;

    // Kalau node_modules/ belum ada, jalankan `npm install` dulu.
    // Ini penting untuk TypeScript (tsc dari devDependencies) dan
    // untuk package yang butuh dependency runtime.
    //
    // cwd = project_root saat dijalankan (dari BuildRunner::runPhase),
    // jadi cukup "npm install" tanpa argumen path.
    if (!fs.exists(nm_path)) {
        out.push_back({
            "npm-install",
            {"npm", "install", "--no-audit", "--no-fund"}
        });
    }

    // Baru jalankan `npm run build` kalau script "build" ada.
    if (packageJsonHasField(txt, "build")) {
        out.push_back({ "npm-build", {"npm", "run", "build"} });
    }

    return out;
}

std::vector<PhaseCommand> NpmBuildSystem::testCommands(
    const ProjectInfo& info, frans::domain::IFileSystemPort&) const
{
    if (info.test_runner != "NpmTest") return {};
    return {{ "npm-test", {"npm", "test", "--silent"} }};
}

std::vector<PhaseCommand> NpmBuildSystem::runCommands(
    const ProjectInfo&, const std::vector<GeneratedFile>&) const
{
    return {};
}

void NpmBuildSystem::parseOutput(
    PhaseResult& result, const ProjectInfo& info) const
{
    if (result.success) return;

    // TypeScript: path(line,col): error TSxxxx: message
    static const std::regex reTs(
        R"re(([^(]+)\((\d+),(\d+)\):\s+(error|warning)\s+TS\d+:\s+([^\n]+))re");

    // npm ERR! untuk error non-TS (mis. install failure).
    static const std::regex reNpmErr(R"re(npm ERR!\s+([^\n]+))re");

    for (const auto& cmd : result.commands) {
        const std::string& src = !cmd.stdout_tail.empty()
                               ? cmd.stdout_tail : cmd.stderr_tail;

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

        // Kalau tidak ada TS error, coba npm ERR! sebagai fallback
        // (biasanya dari npm install yang gagal).
        std::smatch m;
        if (std::regex_search(src, m, reNpmErr)) {
            result.error_message = "npm: " + m[1].str();
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
    result.error_message = "npm command failed.";
}

} // namespace frans::application