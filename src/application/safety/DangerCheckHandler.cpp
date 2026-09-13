// ~/AgenAI/frans/src/application/safety/DangerCheckHandler.cpp
#include "application/safety/DangerCheckHandler.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace frans::application {

namespace {

// ---------------------------------------------------------------------------
// Pattern list — Python
// ---------------------------------------------------------------------------
//
// Kunci: substring yang dicari. Nilai: verdict kalau substring itu ada.
//
// Setiap pattern di sini adalah substring yang cukup spesifik — tidak
// akan muncul kecuali programmer/LLM memang berniat memanggil API
// tersebut. Kalau ragu, masukkan ke NeedsConfirm, bukan Blocked.
const std::unordered_map<std::string, DangerVerdict>& pythonPatterns() {
    static const std::unordered_map<std::string, DangerVerdict> kPatterns = {
        // ---- Shell & subprocess (sering sah, tapi perlu eksplisit) ----
        {"os.system",                DangerVerdict::NeedsConfirm},
        {"os.popen",                 DangerVerdict::NeedsConfirm},
        {"os.spawn",                 DangerVerdict::NeedsConfirm},
        {"os.exec",                  DangerVerdict::NeedsConfirm},
        {"os.fork",                  DangerVerdict::NeedsConfirm},
        {"subprocess.",              DangerVerdict::NeedsConfirm},

        // ---- Dynamic code execution ----
        {"eval(",                    DangerVerdict::NeedsConfirm},
        {"exec(",                    DangerVerdict::NeedsConfirm},
        {"__import__(",              DangerVerdict::NeedsConfirm},
        {"pickle.loads",             DangerVerdict::NeedsConfirm},
        {"marshal.loads",            DangerVerdict::NeedsConfirm},

        // ---- File system destruction ----
        {"shutil.rmtree",            DangerVerdict::NeedsConfirm},
        {"os.removedirs",            DangerVerdict::NeedsConfirm},
        {"os.remove(",               DangerVerdict::NeedsConfirm},
        {"os.unlink(",               DangerVerdict::NeedsConfirm},
        {"os.rmdir(",                DangerVerdict::NeedsConfirm},
        {"os.truncate(",             DangerVerdict::NeedsConfirm},

        // ---- Network (dibuat eksplisit supaya tidak lolos diam-diam) ----
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

// Path absolut yang langsung di-Blocked. Sangat spesifik: tidak ada
// skenario wajar user script menulis ke file-file ini.
const std::vector<std::string>& blockedAbsolutePaths() {
    static const std::vector<std::string> kPaths = {
        "/etc/passwd",
        "/etc/shadow",
        "/etc/sudoers",
        "/etc/sudoers.d/",
    };
    return kPaths;
}

// Prefix path absolut yang dianggap mencurigakan → NeedsConfirm.
// Cek pembukaan string literal (' atau ") langsung diikuti prefix,
// supaya tidak false-positive terhadap variabel bernama "boot" dsb.
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

// Base command yang dianggap berbahaya kalau muncul sebagai argv[0].
// Hanya diperiksa basename — "rm" dan "/bin/rm" keduanya tertangkap.
const std::unordered_set<std::string>& dangerousBaseCommands() {
    static const std::unordered_set<std::string> kCmds = {
        "rm", "dd", "mkfs", "mkfs.ext4", "mkfs.xfs", "mkfs.btrfs",
        "shred", "wipefs", "fdisk", "parted", "mkswap", "swapoff",
        "swapon", "reboot", "shutdown", "poweroff", "halt", "init",
        "chown",
    };
    return kCmds;
}

std::string basename(const std::string& path) {
    const auto pos = path.find_last_of('/');
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

// ---------------------------------------------------------------------------
// checkCode
// ---------------------------------------------------------------------------
DangerCheckResult DangerCheckHandler::checkCode(
    const std::string& code,
    const std::string& language) const
{
    DangerCheckResult r;

    if (language != "python") {
        // Belum ada pattern untuk bahasa lain. Sengaja return Safe
        // (bukan NeedsConfirm) supaya stage selanjutnya tidak
        // terblokir — user tetap punya sandbox + timeout sebagai
        // pengaman.
        return r;
    }

    bool any_blocked = false;
    bool any_confirm = false;

    // 1. Blocked path (spesifik, prioritas tertinggi).
    for (const auto& needle : blockedAbsolutePaths()) {
        if (code.find(needle) != std::string::npos) {
            r.matched_patterns.push_back(needle);
            any_blocked = true;
        }
    }

    // 2. Suspicious path prefix (butuh konfirmasi).
    //    Cek literal string: ' atau " langsung diikuti prefix.
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

    // 3. Pattern API berbahaya (verdict per pattern).
    for (const auto& [needle, verdict] : pythonPatterns()) {
        if (code.find(needle) == std::string::npos) continue;
        r.matched_patterns.push_back(needle);
        if (verdict == DangerVerdict::Blocked)      any_blocked = true;
        else if (verdict == DangerVerdict::NeedsConfirm) any_confirm = true;
    }

    // 4. Deduplikasi (path prefix bisa tumpang-tindih dengan pattern
    //    spesifik). Sort supaya output deterministik untuk log.
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

// ---------------------------------------------------------------------------
// checkCommand
// ---------------------------------------------------------------------------
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
        // Flag klasik "rm -rf /".
        if ((a == "-rf" || a == "-fr" || a == "--no-preserve-root")
            && base == "rm")
        {
            r.matched_patterns.push_back("flag:" + a);
            r.verdict = DangerVerdict::NeedsConfirm;
        }

        // Path sistem kritis di argumen manapun → Blocked.
        for (const auto& blocked : blockedAbsolutePaths()) {
            if (a.find(blocked) != std::string::npos) {
                r.matched_patterns.push_back("arg:" + a);
                r.verdict = DangerVerdict::Blocked;
                r.explanation = "Argumen menyentuh path sistem kritis: '"
                              + a + "'";
                return r;   // blocked = final, tidak perlu lanjut scan
            }
        }
    }

    if (!r.matched_patterns.empty() && r.verdict == DangerVerdict::NeedsConfirm) {
        r.explanation = "Perintah berisiko: " + joinPatterns(r.matched_patterns);
    }
    return r;
}

} // namespace frans::application
