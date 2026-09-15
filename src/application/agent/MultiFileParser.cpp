// ~/AgenAI/frans/src/application/agent/MultiFileParser.cpp
#include "application/agent/MultiFileParser.h"

#include "application/agent/CodeExtraction.h"

#include <cctype>
#include <cstdio>
#include <utility>

namespace frans::application {

namespace {

std::string toLowerAscii(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string trimLeft(std::string s) {
    std::size_t i = 0;
    while (i < s.size()
           && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) {
        ++i;
    }
    return s.substr(i);
}

std::string trimRight(std::string s) {
    while (!s.empty()
           && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t p = 0;
    while (p < text.size()) {
        const std::size_t nl = text.find('\n', p);
        if (nl == std::string::npos) {
            lines.emplace_back(text.substr(p));
            break;
        }
        lines.emplace_back(text.substr(p, nl - p));
        p = nl + 1;
    }
    return lines;
}

// ---------------------------------------------------------------------------
// matchFileMarker — Stage 11b-fix
//
// SEBELUMNYA: cek langsung `line.substr(0, 5) == "file:"` — marker HARUS
// persis di kolom 0. Paste dari web browser / chat UI / terminal sering
// menambahkan indentasi (leading spaces/tabs) di tiap baris, sehingga
// marker lolos deteksi → fallback single-file → semua konten di-anggap
// 1 file (dan ditulis ke fallback_path).
//
// SESUDAH: trim leading ASCII whitespace dulu, baru cek prefix "File:".
// False-positive tidak signifikan: baris yang setelah trim berawal "File:"
// memang hampir pasti marker (kombinasi huruf f-i-l-e-: di posisi awal
// baris sangat jarang muncul sebagai kata biasa dalam konteks source code).
// ---------------------------------------------------------------------------
bool matchFileMarker(const std::string& line, std::string& out_path) {
    // Skip leading ASCII whitespace (spaces, tabs). Tidak skip '\r' di sini
    // karena '\r' hanya muncul di akhir baris (CRLF) — sudah di-handle oleh
    // trimRight di bawah.
    std::size_t p = 0;
    while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) {
        ++p;
    }

    if (line.size() - p < 5) return false;

    std::string head = toLowerAscii(line.substr(p, 5));
    if (head != "file:") return false;

    std::string rest = trimLeft(line.substr(p + 5));
    rest = trimRight(std::move(rest));

    while (!rest.empty()
           && (rest.front() == '`' || rest.front() == '"' || rest.front() == '\'')) {
        rest.erase(rest.begin());
    }
    while (!rest.empty()
           && (rest.back() == '`' || rest.back() == '"' || rest.back() == '\'')) {
        rest.pop_back();
    }

    if (rest.empty()) return false;
    out_path = std::move(rest);
    return true;
}

bool isFenceLine(const std::string& line) {
    return line.find("```") != std::string::npos;
}

// ---------------------------------------------------------------------------
// Stage 11b-fix — diagnostic dump.
//
// Dipanggil HANYA kalau input mengandung substring "File:" tapi tidak ada
// marker yang match. Menampilkan 5 baris non-empty pertama supaya user
// bisa langsung lihat apa yang parser lihat (indentasi? bullet? Unicode?
// BOM? dst). Maksimal 5 baris supaya tidak spam log.
// ---------------------------------------------------------------------------
void logFirstLines(const std::vector<std::string>& lines,
                   const char* reason)
{
    std::fprintf(stderr,
        "[frans][multi_file_parser] %s\n", reason);
    std::fprintf(stderr,
        "[frans][multi_file_parser] dumping first non-empty lines:\n");
    int shown = 0;
    for (const auto& l : lines) {
        if (l.empty()) continue;
        // Tampilkan dengan penanda visual spasi leading (·=space, →=tab)
        // supaya indentasi tidak "hilang" di output terminal.
        std::string visible;
        visible.reserve(l.size());
        for (char c : l) {
            if (c == '\t')       visible += "\u2192";     // →
            else if (c == ' ')   visible += "\u00b7";     // ·
            else                 visible += c;
        }
        std::fprintf(stderr, "[frans][multi_file_parser]   | %s\n",
                     visible.c_str());
        if (++shown >= 5) break;
    }
    std::fflush(stderr);
}

} // namespace

MultiFileParseResult parseModelOutput(const std::string& output,
                                       const std::string& fallback_path)
{
    MultiFileParseResult r;

    // Stage 11b-fix — strip UTF-8 BOM kalau ada.
    //
    // BOM adalah 3 byte (0xEF 0xBB 0xBF) di awal string yang secara visual
    // tidak terlihat, tapi menggeser kolom line pertama sehingga
    // line.substr(0, 5) tidak match "File:" (jadi "\xEF\xBB\xBFFi").
    // Ini sering muncul kalau user paste dari editor yang menyimpan dengan
    // BOM atau dari clipboard manager tertentu.
    std::string normalized = output;
    if (normalized.size() >= 3
        && static_cast<unsigned char>(normalized[0]) == 0xEF
        && static_cast<unsigned char>(normalized[1]) == 0xBB
        && static_cast<unsigned char>(normalized[2]) == 0xBF)
    {
        normalized.erase(0, 3);
    }

    const auto lines = splitLines(normalized);

    std::string current_path;
    std::string current_content;
    bool        in_block      = false;   // sudah lihat marker File:
    bool        in_fence      = false;   // sedang di dalam fenced block
    bool        fence_started = false;   // fence pembuka sudah pernah dilihat
                                         // untuk block saat ini
    bool        any_marker_seen = false;

    // Flush block yang SEDANG aktif. Hanya push kalau block benar-benar
    // selesai (fence pembuka + fence penutup sudah dilihat). Block yang
    // belum pernah masuk fence TIDAK akan di-push — itu malformed.
    auto flush_current = [&]() {
        if (!current_path.empty() && fence_started) {
            while (!current_content.empty() && current_content.back() == '\n') {
                current_content.pop_back();
            }
            GeneratedFile f;
            f.relative_path = current_path;
            f.content       = std::move(current_content);
            r.files.push_back(std::move(f));
        }
        current_path.clear();
        current_content.clear();
        in_block      = false;
        in_fence      = false;
        fence_started = false;
    };

    for (const auto& raw : lines) {
        // ---- Di luar block: cari marker File: ----
        if (!in_block) {
            std::string p;
            if (matchFileMarker(raw, p)) {
                current_path    = std::move(p);
                in_block        = true;
                fence_started   = false;
                any_marker_seen = true;
                continue;
            }
            continue;
        }

        // ---- Di dalam block, sebelum fence pembuka ----
        if (!in_fence) {
            if (isFenceLine(raw)) {
                in_fence      = true;
                fence_started = true;
            }
            // Baris non-fence sebelum fence pembuka diabaikan.
            continue;
        }

        // ---- in_fence == true ----
        if (isFenceLine(raw)) {
            // Fence penutup — block selesai. Flush.
            flush_current();
            continue;
        }

        current_content += raw;
        current_content += '\n';
    }

    // ---- EOF handling ----
    // Kalau masih ada block aktif di EOF, berarti malformed:
    //   - tidak pernah lihat fence pembuka, atau
    //   - fence pembuka sudah dilihat tapi belum ditutup.
    // JANGAN flush block yang malformed — itu akan menghasilkan file
    // dengan content kosong / setengah.
    if (in_block) {
        if (fence_started) {
            r.error = "Fence started but not closed for: " + current_path;
        } else {
            r.error = "Marker 'File:' without fenced block for: " + current_path;
        }
    }

    if (any_marker_seen) {
        r.multi_file = true;
        // Jangan overwrite error yang lebih spesifik dari EOF handling.
        if (r.files.empty() && r.error.empty()) {
            r.error = "Marker 'File:' ditemukan, tapi tidak ada fenced block "
                      "yang lengkap setelahnya.";
        }
        return r;
    }

    // ---- Stage 11b-fix — diagnostic sebelum fallback single-file ----
    //
    // Kalau input mengandung "File:" / "file:" / "FILE:" di suatu tempat
    // (tapi tidak match sebagai marker), kemungkinan besar user MEMANG
    // berniat multi-file tapi formatnya tidak dikenali (bullet, bold markdown,
    // Unicode, dst). Dump 5 baris pertama supaya user bisa langsung lihat
    // apa yang parser lihat.
    //
    // Cek case-insensitive: cari salah satu dari tiga varian.
    const bool looks_like_multi =
        (normalized.find("File:") != std::string::npos)
     || (normalized.find("file:") != std::string::npos)
     || (normalized.find("FILE:") != std::string::npos);

    if (looks_like_multi) {
        logFirstLines(lines,
            "input contains 'File:' substring but no marker matched — "
            "kemungkinan ada prefix tak terduga (bullet? bold? unicode?)");
    }

    // ---- Fallback single-file ----
    std::string code;
    if (!extractFencedCodeBlock(normalized, code)) {
        r.error = "Tidak ditemukan fenced code block di response model.";
        return r;
    }
    if (fallback_path.empty()) {
        r.error = "Response single-file, tapi fallback_path kosong.";
        return r;
    }

    GeneratedFile f;
    f.relative_path = fallback_path;
    f.content       = std::move(code);
    r.files.push_back(std::move(f));
    r.multi_file = false;
    return r;
}

} // namespace frans::application
