#include "application/git/GitFacade.h"
#include "infrastructure/system/ProcessExecutor.h"
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

infrastructure::ProcessResult runGitCommand(
    const std::string& project_root,
    const std::vector<std::string>& args,
    std::atomic<bool>* cancel,
    int timeout_sec = 30)
{
    std::vector<std::string> argv = {"git"};
    argv.insert(argv.end(), args.begin(), args.end());
    
    std::vector<std::string> env_extra = {"GIT_TERMINAL_PROMPT=0"};
    
    return infrastructure::runCommand(
        argv,
        timeout_sec,
        {},  // stdin_data
        project_root,  // working_dir
        cancel,
        env_extra
    );
}

std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
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
// Operasi git via ProcessExecutor — implementasi Fase D #68
// ---------------------------------------------------------------------------

GitStatus GitFacade::status(std::atomic<bool>* cancel) {
    GitStatus result;
    
    // Jalankan git status --porcelain=v1 -b
    auto cmd_result = runGitCommand(
        project_root_,
        {"status", "--porcelain=v1", "-b"},
        cancel
    );
    
    if (cmd_result.cancelled_by_user) {
        result.error = "Dibatalkan oleh pengguna";
        return result;
    }
    
    if (cmd_result.timed_out) {
        result.error = "Timeout saat menjalankan git status";
        return result;
    }
    
    // Cek exit code dan stderr untuk deteksi error
    if (cmd_result.exit_code != 0) {
        std::string stderr_lower = toLower(cmd_result.stderr_text);
        
        if (stderr_lower.find("not a git repository") != std::string::npos) {
            result.is_repo = false;
            result.git_available = true;
            result.error = "Bukan git repository: " + project_root_;
        } else if (stderr_lower.find("command not found") != std::string::npos ||
                   stderr_lower.find("no such file or directory") != std::string::npos ||
                   cmd_result.exit_code == 127) {
            result.git_available = false;
            result.error = "git tidak terinstal atau tidak ada di PATH";
        } else {
            result.error = "git status gagal: " + cmd_result.stderr_text;
        }
        return result;
    }
    
    // Parse output
    result = parseStatusPorcelain(cmd_result.stdout_text);
    result.git_available = true;
    
    return result;
}

GitOpResult GitFacade::addAll(std::atomic<bool>* cancel) {
    GitOpResult result;
    
    auto cmd_result = runGitCommand(
        project_root_,
        {"add", "-A"},
        cancel
    );
    
    result.exit_code = cmd_result.exit_code;
    result.output = cmd_result.stdout_text;
    result.error = cmd_result.stderr_text;
    result.cancelled = cmd_result.cancelled_by_user;
    
    if (cmd_result.timed_out) {
        result.ok = false;
        result.error = "Timeout saat menjalankan git add";
        return result;
    }
    
    if (cmd_result.cancelled_by_user) {
        result.ok = false;
        result.error = "Dibatalkan oleh pengguna";
        return result;
    }
    
    result.ok = (cmd_result.exit_code == 0);
    
    if (!result.ok && result.error.empty()) {
        result.error = "git add gagal dengan exit code " + std::to_string(cmd_result.exit_code);
    }
    
    return result;
}

GitOpResult GitFacade::commit(const std::string& message, std::atomic<bool>* cancel) {
    GitOpResult result;
    
    // Cek apakah ada perubahan yang di-stage
    auto check_result = runGitCommand(
        project_root_,
        {"diff", "--cached", "--name-only"},
        cancel,
        10
    );
    
    if (check_result.exit_code == 0) {
        std::string output = trim(check_result.stdout_text);
        
        if (output.empty()) {
            result.ok = false;
            result.error = "Tidak ada perubahan yang di-stage untuk di-commit. Gunakan 'git add' terlebih dahulu.";
            return result;
        }
    }
    
    auto cmd_result = runGitCommand(
        project_root_,
        {"commit", "-m", message},
        cancel
    );
    
    result.exit_code = cmd_result.exit_code;
    result.output = cmd_result.stdout_text;
    result.error = cmd_result.stderr_text;
    result.cancelled = cmd_result.cancelled_by_user;
    
    if (cmd_result.timed_out) {
        result.ok = false;
        result.error = "Timeout saat menjalankan git commit";
        return result;
    }
    
    if (cmd_result.cancelled_by_user) {
        result.ok = false;
        result.error = "Dibatalkan oleh pengguna";
        return result;
    }
    
    result.ok = (cmd_result.exit_code == 0);
    
    if (!result.ok && result.error.empty()) {
        result.error = "git commit gagal dengan exit code " + std::to_string(cmd_result.exit_code);
    }
    
    return result;
}

GitOpResult GitFacade::push(std::atomic<bool>* cancel) {
    GitOpResult result;
    
    auto cmd_result = runGitCommand(
        project_root_,
        {"push"},
        cancel,
        60  // push bisa lebih lama karena network
    );
    
    result.exit_code = cmd_result.exit_code;
    result.output = cmd_result.stdout_text;
    result.error = cmd_result.stderr_text;
    result.cancelled = cmd_result.cancelled_by_user;
    
    if (cmd_result.timed_out) {
        result.ok = false;
        result.error = "Timeout saat menjalankan git push";
        return result;
    }
    
    if (cmd_result.cancelled_by_user) {
        result.ok = false;
        result.error = "Dibatalkan oleh pengguna";
        return result;
    }
    
    result.ok = (cmd_result.exit_code == 0);
    
    if (!result.ok) {
        std::string error_lower = toLower(result.error);
        
        // Cek error spesifik
        if (error_lower.find("no upstream") != std::string::npos ||
            error_lower.find("has no upstream") != std::string::npos ||
            error_lower.find("set-upstream") != std::string::npos) {
            result.error = "Branch tidak memiliki upstream. Gunakan: git push -u origin <branch>";
        } else if (error_lower.find("authentication failed") != std::string::npos ||
                   error_lower.find("permission denied") != std::string::npos ||
                   error_lower.find("could not read username") != std::string::npos) {
            result.error = "Autentikasi gagal. Periksa kredensial git Anda (SSH key atau Git Credential Manager).";
        } else if (error_lower.find("rejected") != std::string::npos &&
                   error_lower.find("non-fast-forward") != std::string::npos) {
            result.error = "Push ditolak: branch remote lebih baru. Pull terlebih dahulu atau gunakan force push (tidak disarankan).";
        } else if (error_lower.find("everything up-to-date") != std::string::npos) {
            // Ini sebenarnya bukan error, tapi git mengembalikan exit code 0
            result.ok = true;
            result.error = "Everything up-to-date";
        } else if (result.error.empty()) {
            result.error = "git push gagal dengan exit code " + std::to_string(cmd_result.exit_code);
        }
    }
    
    return result;
}

} // namespace frans::application