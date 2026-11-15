#include "application/safety/DangerCheckHandler.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace frans::application {
namespace {

const std::unordered_map<std::string, DangerVerdict>& pythonPatterns() {
    static const std::unordered_map<std::string, DangerVerdict> kPatterns = {
        // Shell & subprocess
        {"os.system",                DangerVerdict::NeedsConfirm},
        {"os.popen",                 DangerVerdict::NeedsConfirm},
        {"os.spawn",                 DangerVerdict::NeedsConfirm},
        {"os.exec",                  DangerVerdict::NeedsConfirm},
        {"os.fork",                  DangerVerdict::NeedsConfirm},
        {"subprocess.",              DangerVerdict::NeedsConfirm},
        // Dynamic code
        {"eval(",                    DangerVerdict::NeedsConfirm},
        {"exec(",                    DangerVerdict::NeedsConfirm},
        {"__import__(",              DangerVerdict::NeedsConfirm},
        {"pickle.loads",             DangerVerdict::NeedsConfirm},
        {"marshal.loads",            DangerVerdict::NeedsConfirm},
        // Filesystem destruction
        {"shutil.rmtree",            DangerVerdict::NeedsConfirm},
        {"os.removedirs",            DangerVerdict::NeedsConfirm},
        {"os.remove(",               DangerVerdict::NeedsConfirm},
        {"os.unlink(",               DangerVerdict::NeedsConfirm},
        {"os.rmdir(",                DangerVerdict::NeedsConfirm},
        {"os.truncate(",             DangerVerdict::NeedsConfirm},
        // Network
        {"socket.socket",            DangerVerdict::NeedsConfirm},
        {"socket.create_connection", DangerVerdict::NeedsConfirm},
        {"urllib.request",           DangerVerdict::NeedsConfirm},
        {"requests.get",             DangerVerdict::NeedsConfirm},
        {"requests.post",            DangerVerdict::NeedsConfirm},
        {"http.client",              DangerVerdict::NeedsConfirm},
        {"ftplib.",                  DangerVerdict::NeedsConfirm},
        {"smtplib.",                 DangerVerdict::NeedsConfirm},
    };
    return kPatterns;
}

const std::vector<std::string>& blockedLinuxPaths() {
    static const std::vector<std::string> kPaths = {
        "/etc/passwd",
        "/etc/shadow",
        "/etc/sudoers",
        "/etc/sudoers.d/",
    };
    return kPaths;
}

const std::vector<std::string>& blockedWindowsPaths() {
    static const std::vector<std::string> kPaths = {
        "C:/Windows/System32/config/SAM",
        "C:/Windows/System32/config/SECURITY",
        "C:/Windows/System32/config/SYSTEM",
        "C:/Windows/System32/drivers/etc/hosts",
        "AppData/Local/Microsoft/Credentials",
    };
    return kPaths;
}

const std::vector<std::string>& suspiciousPathPrefixes() {
    static const std::vector<std::string> kPrefixes = {
        "/etc/",
        "/root/",
        "/boot/",
        "/sys/kernel/",
        "/proc/sys/",
    };
    return kPrefixes;
}

const std::unordered_set<std::string>& dangerousBaseCommands() {
    static const std::unordered_set<std::string> kCmds = {
        // Linux
        "rm", "dd", "mkfs", "mkfs.ext4", "mkfs.xfs", "mkfs.btrfs",
        "shred", "wipefs", "fdisk", "parted", "mkswap", "swapoff",
        "swapon", "reboot", "shutdown", "poweroff", "halt", "init",
        "chown",
        // Windows
        "format", "diskpart", "reg", "shutdown.exe", "taskkill",
    };
    return kCmds;
}

std::string basename(const std::string& path) {
    const auto pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string joinPatterns(const std::vector<std::string>& v) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) s += ", ";
        s += "'" + v[i] + "'";
    }
    return s;
}

bool isGitDangerousCommand(const std::vector<std::string>& argv,
                           std::string& pattern_out) {
    if (argv.empty()) return false;

    std::string base = basename(argv[0]);
    std::string lower_base = base;
    std::transform(lower_base.begin(), lower_base.end(), lower_base.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    // Handle both 'git' and 'git.exe' (case-insensitive for Windows)
    if (lower_base != "git" && lower_base != "git.exe") {
        return false;
    }

    // Find the subcommand, skipping global flags and their arguments
    std::string subcmd;
    std::size_t subcmd_idx = 0;
    for (std::size_t i = 1; i < argv.size(); ++i) {
        const std::string& a = argv[i];
        if (a.empty()) continue;
        
        if (a[0] == '-') {
            // Global flags that take a separate argument value
            if (a == "-C" || a == "--git-dir" || a == "--work-tree" || 
                a == "--namespace" || a == "-c" || a == "--config") {
                ++i; // skip the next element which is the argument value
            }
            continue; // skip the flag itself
        }
        
        // First non-flag argument is the subcommand
        subcmd = a;
        subcmd_idx = i;
        break;
    }

    if (subcmd.empty()) return false;

    auto hasFlag = [&](const std::string& flag) {
        for (std::size_t i = subcmd_idx + 1; i < argv.size(); ++i) {
            if (argv[i] == flag) return true;
        }
        return false;
    };

    auto hasDotPath = [&]() {
        for (std::size_t i = subcmd_idx + 1; i < argv.size(); ++i) {
            if (argv[i] == ".") return true;
        }
        return false;
    };

    auto hasCredentialArg = [&]() {
        for (std::size_t i = subcmd_idx + 1; i < argv.size(); ++i) {
            std::string lower = argv[i];
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (lower.find("credential") != std::string::npos) return true;
        }
        return false;
    };

    if (subcmd == "push") {
        if (hasFlag("--force") || hasFlag("-f") || hasFlag("--force-with-lease")) {
            pattern_out = "git:push (force)";
            return true;
        }
    } else if (subcmd == "reset") {
        if (hasFlag("--hard")) {
            pattern_out = "git:reset --hard";
            return true;
        }
    } else if (subcmd == "clean") {
        if (hasFlag("-f") || hasFlag("--force")) {
            pattern_out = "git:clean (force)";
            return true;
        }
    } else if (subcmd == "checkout" || subcmd == "restore") {
        if (hasDotPath()) {
            pattern_out = "git:" + subcmd + " .";
            return true;
        }
    } else if (subcmd == "config") {
        if (hasCredentialArg()) {
            pattern_out = "git:config (credential)";
            return true;
        }
    }

    return false;
}

} // namespace

DangerCheckResult DangerCheckHandler::checkCode(
    const std::string& code,
    const std::string& language) const
{
    DangerCheckResult r;

    if (language != "python") return r;

    bool any_blocked = false;
    bool any_confirm = false;

    for (const auto& needle : blockedLinuxPaths()) {
        if (code.find(needle) != std::string::npos) {
            r.matched_patterns.push_back(needle);
            any_blocked = true;
        }
    }

    for (const auto& needle : blockedWindowsPaths()) {
        if (code.find(needle) != std::string::npos) {
            r.matched_patterns.push_back(needle);
            any_blocked = true;
        }
    }

    for (const auto& pfx : suspiciousPathPrefixes()) {
        const std::string dq = "\"" + pfx;
        const std::string sq = "'"  + pfx;
        if (code.find(dq) != std::string::npos) {
            r.matched_patterns.push_back(dq + "...");
            any_confirm = true;
        }
        if (code.find(sq) != std::string::npos) {
            r.matched_patterns.push_back(sq + "...");
            any_confirm = true;
        }
    }

    for (const auto& [needle, verdict] : pythonPatterns()) {
        if (code.find(needle) == std::string::npos) continue;
        r.matched_patterns.push_back(needle);
        if (verdict == DangerVerdict::Blocked)                any_blocked = true;
        else if (verdict == DangerVerdict::NeedsConfirm)      any_confirm = true;
    }

    std::sort(r.matched_patterns.begin(), r.matched_patterns.end());
    r.matched_patterns.erase(
        std::unique(r.matched_patterns.begin(), r.matched_patterns.end()),
        r.matched_patterns.end());

    if (r.matched_patterns.empty()) {
        r.verdict = DangerVerdict::Safe;
        return r;
    }

    if (any_blocked) {
        r.verdict = DangerVerdict::Blocked;
        r.explanation = "Kode mengandung referensi ke path sistem kritis: "
                      + joinPatterns(r.matched_patterns);
    } else if (any_confirm) {
        r.verdict = DangerVerdict::NeedsConfirm;
        r.explanation = "Kode mengandung pola berisiko: "
                      + joinPatterns(r.matched_patterns);
    }
    return r;
}

DangerCheckResult DangerCheckHandler::checkCommand(
    const std::vector<std::string>& argv) const
{
    DangerCheckResult r;
    if (argv.empty()) return r;

    std::string git_pattern;
    if (isGitDangerousCommand(argv, git_pattern)) {
        r.matched_patterns.push_back(git_pattern);
        r.verdict = DangerVerdict::NeedsConfirm;
    }

    const std::string base = basename(argv[0]);
    if (dangerousBaseCommands().count(base) > 0) {
        r.matched_patterns.push_back("cmd:" + base);
        r.verdict = DangerVerdict::NeedsConfirm;
    }

    for (const auto& a : argv) {
        if ((a == "-rf" || a == "-fr" || a == "--no-preserve-root")
            && base == "rm")
        {
            r.matched_patterns.push_back("flag:" + a);
            r.verdict = DangerVerdict::NeedsConfirm;
        }
        for (const auto& blocked : blockedLinuxPaths()) {
            if (a.find(blocked) != std::string::npos) {
                r.matched_patterns.push_back("arg:" + a);
                r.verdict = DangerVerdict::Blocked;
                r.explanation = "Argumen menyentuh path sistem kritis: '"
                              + a + "'";
                return r;
            }
        }
        for (const auto& blocked : blockedWindowsPaths()) {
            if (a.find(blocked) != std::string::npos) {
                r.matched_patterns.push_back("arg:" + a);
                r.verdict = DangerVerdict::Blocked;
                r.explanation = "Argumen menyentuh path sistem kritis: '"
                              + a + "'";
                return r;
            }
        }
    }

    if (!r.matched_patterns.empty()
        && r.verdict == DangerVerdict::NeedsConfirm)
    {
        r.explanation = "Perintah berisiko: "
                      + joinPatterns(r.matched_patterns);
    }
    return r;
}

} // namespace frans::application