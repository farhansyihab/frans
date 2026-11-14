#include "application/git/GitFacade.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace frans::application {

namespace {

std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(static_cast<unsigned char>(*start))) ++start;
    if (start == s.end()) return {};
    auto end = s.end();
    do { --end; } while (std::distance(start, end) > 0 &&
                         std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

std::string unquotePath(const std::string& path) {
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        std::string result;
        result.reserve(path.size() - 2);
        for (size_t i = 1; i + 1 < path.size(); ++i) {
            if (path[i] == '\\' && i + 1 < path.size() - 1) {
                char next = path[i + 1];
                if (next == '"' || next == '\\' || next == 'n' || next == 't') {
                    if (next == 'n') result += '\n';
                    else if (next == 't') result += '\t';
                    else result += next;
                    ++i;
                    continue;
                }
            }
            result += path[i];
        }
        return result;
    }
    return path;
}

bool isSuspiciousPath(const std::string& path) {
    static const std::vector<std::string> suspicious_patterns = {
        ".env", ".env.", "credentials", "secret", "password",
        "private_key", "id_rsa", "id_ed25519", ".pem", ".key"
    };

    std::string lower_path = path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& pattern : suspicious_patterns) {
        if (lower_path.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

GitFacade::GitFacade(std::string project_root)
    : project_root_(std::move(project_root)) {}

GitStatus GitFacade::parseStatusPorcelain(const std::string& text) {
    GitStatus status;

    if (text.empty()) {
        status.error = "Empty status output";
        return status;
    }

    std::istringstream stream(text);
    std::string line;
    bool first_line = true;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        if (first_line) {
            first_line = false;

            if (line.size() < 3 || line.substr(0, 3) != "## ") {
                status.error = "Invalid porcelain format: missing branch header";
                return status;
            }

            status.is_repo = true;

            std::string branch_info = line.substr(3);

            auto dots_pos = branch_info.find("...");
            if (dots_pos != std::string::npos) {
                status.branch = branch_info.substr(0, dots_pos);
                std::string rest = branch_info.substr(dots_pos + 3);

                auto bracket_pos = rest.find('[');
                if (bracket_pos != std::string::npos) {
                    status.upstream = trim(rest.substr(0, bracket_pos));
                    std::string counts = rest.substr(bracket_pos + 1);

                    std::smatch match;
                    if (std::regex_search(counts, match, std::regex("ahead (\\d+)"))) {
                        status.ahead = std::stoi(match[1].str());
                    }
                    if (std::regex_search(counts, match, std::regex("behind (\\d+)"))) {
                        status.behind = std::stoi(match[1].str());
                    }
                } else {
                    status.upstream = trim(rest);
                }
            } else {
                status.branch = trim(branch_info);
            }

            continue;
        }

        if (line.size() < 3) continue;

        GitFileChange change;
        change.index_status    = line[0];
        change.worktree_status = line[1];

        std::string path_part = line.substr(3);

        // Renamed: "R  old -> new" — ambil path baru saja.
        auto arrow_pos = path_part.find(" -> ");
        if (arrow_pos != std::string::npos) {
            path_part = path_part.substr(arrow_pos + 4);
        }

        change.path       = unquotePath(trim(path_part));
        change.suspicious = isSuspiciousPath(change.path);

        status.changes.push_back(std::move(change));
    }

    return status;
}

std::string GitFacade::suggestCommitMessage(const GitStatus& st) {
    if (st.changes.empty()) {
        return "No changes to commit";
    }

    int added = 0, modified = 0, deleted = 0, renamed = 0, untracked = 0;

    for (const auto& change : st.changes) {
        char s = change.index_status != ' ' ? change.index_status : change.worktree_status;
        switch (s) {
            case 'A': ++added;     break;
            case 'M': ++modified;  break;
            case 'D': ++deleted;   break;
            case 'R': ++renamed;   break;
            case '?': ++untracked; break;
            default: break;
        }
    }

    std::ostringstream msg;

    if (st.changes.size() == 1) {
        const auto& change = st.changes[0];
        char s = change.index_status != ' ' ? change.index_status : change.worktree_status;

        auto last_slash = change.path.find_last_of('/');
        std::string filename = (last_slash != std::string::npos)
            ? change.path.substr(last_slash + 1)
            : change.path;

        switch (s) {
            case 'A': msg << "Add "            << filename; break;
            case 'M': msg << "Update "         << filename; break;
            case 'D': msg << "Remove "         << filename; break;
            case 'R': msg << "Rename "         << filename; break;
            case '?': msg << "Add untracked "  << filename; break;
            default:  msg << "Update "         << filename; break;
        }
    } else {
        std::vector<std::string> actions;
        if (added     > 0) actions.push_back(std::to_string(added)     + " added");
        if (modified  > 0) actions.push_back(std::to_string(modified)  + " modified");
        if (deleted   > 0) actions.push_back(std::to_string(deleted)   + " deleted");
        if (renamed   > 0) actions.push_back(std::to_string(renamed)   + " renamed");
        if (untracked > 0) actions.push_back(std::to_string(untracked) + " untracked");

        std::ostringstream detail;
        detail << "Update " << st.changes.size() << " files (";
        for (size_t i = 0; i < actions.size(); ++i) {
            if (i > 0) detail << ", ";
            detail << actions[i];
        }
        detail << ")";

        // Conventional commit line length ~72 chars.
        if (detail.str().size() > 72) {
            msg << "Update " << st.changes.size() << " files";
        } else {
            msg << detail.str();
        }
    }

    return msg.str();
}

// ---------------------------------------------------------------------------
// Operasi git via ProcessExecutor — belum diimplementasikan di Fase D #67.
// Akan diisi di item #68.
// ---------------------------------------------------------------------------

GitStatus GitFacade::status(std::atomic<bool>* /*cancel*/) {
    GitStatus result;
    result.error = "Not implemented in Fase D #67 (pure functions only)";
    return result;
}

GitOpResult GitFacade::addAll(std::atomic<bool>* /*cancel*/) {
    GitOpResult result;
    result.error = "Not implemented in Fase D #67";
    return result;
}

GitOpResult GitFacade::commit(const std::string& /*message*/,
                              std::atomic<bool>* /*cancel*/) {
    GitOpResult result;
    result.error = "Not implemented in Fase D #67";
    return result;
}

GitOpResult GitFacade::push(std::atomic<bool>* /*cancel*/) {
    GitOpResult result;
    result.error = "Not implemented in Fase D #67";
    return result;
}

} // namespace frans::application