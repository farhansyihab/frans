#include "application/safety/DangerCheckHandler.h"

#include <algorithm>
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

// Path sistem Linux kritis -> Blocked.
const std::vector<std::string>& blockedLinuxPaths() {
    static const std::vector<std::string> kPaths = {
        "/etc/passwd",
        "/etc/shadow",
        "/etc/sudoers",
        "/etc/sudoers.d/",
    };
    return kPaths;
}

// Path sistem Windows kritis -> Blocked.
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

// Prefix path yang mencurigakan -> NeedsConfirm.
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

// Base command berbahaya (Linux + Windows).
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

} // namespace

DangerCheckResult DangerCheckHandler::checkCode(
    const std::string& code,
    const std::string& language) const
{
    DangerCheckResult r;

    // Saat ini pattern hanya untuk Python.
    if (language != "python") return r;

    bool any_blocked = false;
    bool any_confirm = false;

    // Linux blocked paths.
    for (const auto& needle : blockedLinuxPaths()) {
        if (code.find(needle) != std::string::npos) {
            r.matched_patterns.push_back(needle);
            any_blocked = true;
        }
    }

    // Windows blocked paths.
    for (const auto& needle : blockedWindowsPaths()) {
        if (code.find(needle) != std::string::npos) {
            r.matched_patterns.push_back(needle);
            any_blocked = true;
        }
    }

    // Suspicious path prefixes.
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

    // Pattern API.
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