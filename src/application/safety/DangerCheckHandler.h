#pragma once

#include <string>
#include <vector>

namespace frans::application {

enum class DangerVerdict {
    Safe,
    NeedsConfirm,
    Blocked,
};

struct DangerCheckResult {
    DangerVerdict             verdict = DangerVerdict::Safe;
    std::vector<std::string>  matched_patterns;
    std::string               explanation;

    bool is_safe()       const { return verdict == DangerVerdict::Safe; }
    bool needs_confirm() const { return verdict == DangerVerdict::NeedsConfirm; }
    bool is_blocked()    const { return verdict == DangerVerdict::Blocked; }
};

// Deteksi pola berbahaya di source code dan argv command.
//
// Konservatif: pola umum (open, print, import os) TIDAK ditandai.
// Pola berisiko (subprocess, socket) -> NeedsConfirm.
// Path sistem kritis -> Blocked.
//
// Pattern list multi-bahasa: Python + Windows (SAM, hosts, reg, dll).
class DangerCheckHandler {
public:
    DangerCheckResult checkCode(const std::string& code,
                                const std::string& language) const;

    DangerCheckResult checkCommand(const std::vector<std::string>& argv) const;
};

} // namespace frans::application