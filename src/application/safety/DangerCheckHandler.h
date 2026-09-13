// ~/AgenAI/frans/src/application/safety/DangerCheckHandler.h
#pragma once

#include <string>
#include <vector>

namespace frans::application {

// Tingkat bahaya dari sebuah artefak (kode sumber / argv command).
//
//   Safe         : tidak ada pola mencurigakan, boleh langsung dijalankan.
//   NeedsConfirm : ada pola berisiko (subprocess, socket, dst). Wajib
//                  dikonfirmasi ke user sebelum dieksekusi.
//   Blocked      : ada referensi ke path/sistem kritis. Ditolak mutlak.
enum class DangerVerdict {
    Safe,
    NeedsConfirm,
    Blocked,
};

struct DangerCheckResult {
    DangerVerdict             verdict = DangerVerdict::Safe;
    std::vector<std::string>  matched_patterns;
    std::string               explanation;

    bool is_safe()        const { return verdict == DangerVerdict::Safe; }
    bool needs_confirm()  const { return verdict == DangerVerdict::NeedsConfirm; }
    bool is_blocked()     const { return verdict == DangerVerdict::Blocked; }
};

// Blueprint §4 (Chain of Responsibility) & §5.8 (Safety Layer).
//
// Stateless checker untuk mendeteksi pola berbahaya di:
//   1. Source code yang baru digenerate LLM, SEBELUM dieksekusi.
//   2. argv command yang akan dijalankan via ProcessExecutor.
//
// Pencarian substring, bukan parsing AST. Tujuannya BUKAN menahan
// penyerang cerdas (itu tugas OS-level sandbox), tapi mencegah LLM
// "tidak sengaja" menulis `os.system("rm -rf ~")` atau menyentuh
// /etc/passwd.
//
// Konservatif terhadap false-positive:
//   - Pola umum & aman seperti open(), print(), import os TIDAK ditandai.
//   - Pola yang sering sah tapi berisiko (subprocess, socket) → NeedsConfirm.
//   - Path sistem kritis (/etc/passwd, /etc/shadow, /etc/sudoers) → Blocked.
//
// Catatan: Stage 7c hanya mengimplementasikan pattern list Python. Bahasa
// lain ditambahkan di Stage 10 tanpa perubahan API.
class DangerCheckHandler {
public:
    // Scan source code. `language` = "python" (Stage 7c).
    // Bahasa lain → return Safe (belum ada pattern).
    DangerCheckResult checkCode(const std::string& code,
                                const std::string& language) const;

    // Scan argv command yang akan dijalankan. Cek argv[0] terhadap base
    // command berbahaya (rm, dd, mkfs, ...), dan cek seluruh argumen
    // terhadap path sistem kritis.
    DangerCheckResult checkCommand(const std::vector<std::string>& argv) const;
};

} // namespace frans::application
