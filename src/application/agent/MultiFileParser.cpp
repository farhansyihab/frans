// ~/AgenAI/frans/src/application/agent/MultiFileParser.cpp
#include "application/agent/MultiFileParser.h"

#include "application/agent/CodeExtraction.h"

#include <cctype>
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

bool matchFileMarker(const std::string& line, std::string& out_path) {
    if (line.size() < 5) return false;
    std::string head = toLowerAscii(line.substr(0, 5));
    if (head != "file:") return false;

    std::string rest = trimLeft(line.substr(5));
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

} // namespace

MultiFileParseResult parseModelOutput(const std::string& output,
                                       const std::string& fallback_path)
{
    MultiFileParseResult r;

    const auto lines = splitLines(output);

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

    // ---- Fallback single-file ----
    std::string code;
    if (!extractFencedCodeBlock(output, code)) {
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
