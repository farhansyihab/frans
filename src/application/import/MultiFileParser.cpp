#include "application/import/MultiFileParser.h"

#include <cctype>
#include <cstdio>
#include <utility>

namespace frans::application {

namespace {

// Hindari literal triple-backtick di source (bisa mengganggu tooling).
const std::string kFence = "`" "`" "`";

std::string toLowerAscii(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string trimLeft(std::string s) {
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) ++i;
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
    std::size_t p = 0;
    while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) ++p;
    if (line.size() - p < 5) return false;

    const std::string head = toLowerAscii(line.substr(p, 5));
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
    return line.find(kFence) != std::string::npos;
}

// Ekstrak fenced code block pertama dari `text` (inline dari
// CodeExtraction.h v7, tidak lagi bergantung file eksternal).
bool extractFirstFencedBlock(const std::string& text, std::string& out) {
    const std::size_t open = text.find(kFence);
    if (open == std::string::npos) return false;

    std::size_t nl = text.find('\n', open);
    if (nl == std::string::npos) return false;

    const std::size_t close = text.find(kFence, nl + 1);
    if (close == std::string::npos) return false;

    out = text.substr(nl + 1, close - nl - 1);
    while (!out.empty() && out.back() == '\n') out.pop_back();
    return true;
}

} // namespace

MultiFileParseResult parseModelOutput(const std::string& output,
                                       const std::string& fallback_path)
{
    MultiFileParseResult r;

    // Strip UTF-8 BOM kalau ada.
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
    bool        in_block      = false;
    bool        in_fence      = false;
    bool        fence_started = false;
    bool        any_marker_seen = false;

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

        if (!in_fence) {
            if (isFenceLine(raw)) {
                in_fence      = true;
                fence_started = true;
            }
            continue;
        }

        if (isFenceLine(raw)) {
            flush_current();
            continue;
        }

        current_content += raw;
        current_content += '\n';
    }

    if (in_block) {
        if (fence_started) {
            r.error = "Fence started but not closed for: " + current_path;
        } else {
            r.error = "Marker 'File:' without fenced block for: " + current_path;
        }
    }

    if (any_marker_seen) {
        r.multi_file = true;
        if (r.files.empty() && r.error.empty()) {
            r.error = "Marker 'File:' ditemukan, tapi tidak ada fenced block "
                      "yang lengkap setelahnya.";
        }
        return r;
    }

    // Fallback single-file.
    std::string code;
    if (!extractFirstFencedBlock(normalized, code)) {
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