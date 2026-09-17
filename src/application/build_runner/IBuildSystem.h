#pragma once

#include "application/import/MultiFileParser.h"
#include "application/build_runner/BuildReport.h"
#include "application/build_runner/ProjectInfo.h"
#include "domain/fs/IFileSystemPort.h"

#include <string>
#include <vector>

namespace frans::application {

struct PhaseCommand {
    std::string              label;
    std::vector<std::string> argv;
};

class IBuildSystem {
public:
    virtual ~IBuildSystem() = default;

    virtual std::string name() const = 0;

    virtual std::vector<PhaseCommand> buildCommands(
        const ProjectInfo& info,
        const std::vector<GeneratedFile>& files,
        frans::domain::IFileSystemPort& fs) const = 0;

    virtual std::vector<PhaseCommand> testCommands(
        const ProjectInfo& info,
        frans::domain::IFileSystemPort& fs) const = 0;

    virtual std::vector<PhaseCommand> runCommands(
        const ProjectInfo& info,
        const std::vector<GeneratedFile>& files) const = 0;

    virtual void parseOutput(
        PhaseResult& result,
        const ProjectInfo& info) const = 0;

protected:
    static std::string stripRoot(const std::string& raw,
                                  const std::string& root)
    {
        if (root.empty() || raw.empty()) return raw;
        std::string prefix = root;
        if (prefix.back() != '/') prefix += '/';
        if (raw.size() > prefix.size() && raw.rfind(prefix, 0) == 0) {
            return raw.substr(prefix.size());
        }
        return raw;
    }

    static std::string lastNonEmptyLine(const std::string& s) {
        std::string line, last;
        for (std::size_t p = 0; p < s.size(); ) {
            const std::size_t nl = s.find('\n', p);
            if (nl == std::string::npos) {
                line = s.substr(p);
                p = s.size();
            } else {
                line = s.substr(p, nl - p);
                p = nl + 1;
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) last = line;
        }
        return last;
    }

    static std::string tailString(const std::string& s, std::size_t max_chars) {
        if (s.size() <= max_chars) return s;
        return std::string("...(truncated)...\n")
             + s.substr(s.size() - max_chars);
    }
};

} // namespace frans::application