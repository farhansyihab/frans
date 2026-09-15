// ~/AgenAI/frans/src/code_intelligence/parsers/VueParser.cpp
#include "code_intelligence/parsers/VueParser.h"

#include "code_intelligence/parsers/JavaScriptParser.h"
#include "code_intelligence/parsers/TypeScriptParser.h"

#include <cctype>
#include <cstdio>
#include <string>

namespace frans::code_intelligence {

namespace {

// Case-insensitive substring search. Return posisi awal match, atau
// std::string::npos kalau tidak ditemukan. Sederhana O(n*m) — cukup
// untuk file .vue yang biasanya < 100 KB.
std::size_t findCaseInsensitive(const std::string& hay,
                                 const std::string& needle,
                                 std::size_t start = 0) {
    if (needle.empty() || hay.size() < needle.size()) {
        return std::string::npos;
    }
    const std::size_t last = hay.size() - needle.size();
    for (std::size_t i = start; i <= last; ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const char a = static_cast<char>(
                std::tolower(static_cast<unsigned char>(hay[i + j])));
            const char b = static_cast<char>(
                std::tolower(static_cast<unsigned char>(needle[j])));
            if (a != b) { match = false; break; }
        }
        if (match) return i;
    }
    return std::string::npos;
}

// Hitung nomor baris (1-based) dari byte ke-`pos` di `src`.
// `pos` boleh == src.size() (berarti "setelah byte terakhir").
int lineOfByte(const std::string& src, std::size_t pos) {
    const std::size_t limit = (pos < src.size()) ? pos : src.size();
    int line = 1;
    for (std::size_t i = 0; i < limit; ++i) {
        if (src[i] == '\n') ++line;
    }
    return line;
}

// Ambil value attribute `attr` dari dalam opening tag (bagian antara
// `<tagname` dan `>`).
//
// Handle:
//   attr="value"     → return "value"
//   attr='value'     → return "value"
//   attr=value       → return "value"  (unquoted; rare di Vue)
//   attr             → return ""        (boolean attr, no value)
//   tidak ada        → return ""
//
// Boundary check: attr harus didahului whitespace/start dan diikuti
// whitespace/'='/end — supaya "lang" tidak match ke "language".
std::string extractAttribute(const std::string& tag,
                              const std::string& attr) {
    std::size_t pos = 0;
    while ((pos = tag.find(attr, pos)) != std::string::npos) {
        const bool start_ok =
            (pos == 0) ||
            std::isspace(static_cast<unsigned char>(tag[pos - 1]));

        const std::size_t after = pos + attr.size();
        const bool end_ok =
            (after >= tag.size()) ||
            std::isspace(static_cast<unsigned char>(tag[after])) ||
            (tag[after] == '=');

        if (start_ok && end_ok) {
            std::size_t p = after;
            while (p < tag.size()
                   && std::isspace(static_cast<unsigned char>(tag[p]))) {
                ++p;
            }
            if (p < tag.size() && tag[p] == '=') {
                ++p;
                while (p < tag.size()
                       && std::isspace(static_cast<unsigned char>(tag[p]))) {
                    ++p;
                }
                if (p < tag.size() && (tag[p] == '"' || tag[p] == '\'')) {
                    const char quote = tag[p];
                    ++p;
                    const std::size_t qend = tag.find(quote, p);
                    if (qend != std::string::npos) {
                        return tag.substr(p, qend - p);
                    }
                    return {};   // unterminated quote
                }
                // Unquoted value.
                const std::size_t vstart = p;
                while (p < tag.size()
                       && !std::isspace(static_cast<unsigned char>(tag[p]))
                       && tag[p] != '>') {
                    ++p;
                }
                return tag.substr(vstart, p - vstart);
            }
            // Boolean attribute (no value).
            return {};
        }
        pos = after;
    }
    return {};
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------
VueParser::VueParser()
    : ts_parser_(std::make_unique<TypeScriptParser>(
          TypeScriptParser::Variant::Ts)),
      tsx_parser_(std::make_unique<TypeScriptParser>(
          TypeScriptParser::Variant::Tsx)),
      js_parser_(std::make_unique<JavaScriptParser>())
{}

VueParser::~VueParser() = default;

// ---------------------------------------------------------------------------
// findScriptBlock
// ---------------------------------------------------------------------------
VueParser::ScriptBlock VueParser::findScriptBlock(
    const std::string& source) const
{
    ScriptBlock block;

    // 1. Cari `<script` (case-insensitive).
    const std::size_t open_start = findCaseInsensitive(source, "<script");
    if (open_start == std::string::npos) {
        return block;   // tidak ada <script> — bukan error
    }

    // 2. Boundary check: char setelah "<script" tidak boleh alnum,
    //    untuk menghindari false positive seperti "<scripting>".
    const std::size_t after_name = open_start + 7;   // strlen("<script")
    if (after_name < source.size()
        && std::isalnum(static_cast<unsigned char>(source[after_name]))) {
        return block;
    }

    // 3. Cari `>` penutup opening tag.
    const std::size_t gt_pos = source.find('>', after_name);
    if (gt_pos == std::string::npos) {
        std::fprintf(stderr,
            "[frans][vue] malformed SFC: '<script' tanpa '>' penutup\n");
        std::fflush(stderr);
        return block;
    }

    // 4. Cari `</script` penutup.
    const std::size_t close_pos =
        findCaseInsensitive(source, "</script", gt_pos + 1);
    if (close_pos == std::string::npos) {
        std::fprintf(stderr,
            "[frans][vue] malformed SFC: '<script>' tanpa '</script>'\n");
        std::fflush(stderr);
        return block;
    }

    // 5. Extract atribut dari tag body (antara `<script` dan `>`).
    const std::string tag_attrs =
        source.substr(after_name, gt_pos - after_name);

    // 6. Content = antara `>` dan `</script`. Strip leading
    //    whitespace (termasuk newline) supaya `start_line` menunjuk
    //    ke baris karakter non-whitespace PERTAMA — ini menyelaraskan
    //    relative line 1 dari TS/JS parser dengan baris asli di .vue.
    std::size_t content_start = gt_pos + 1;
    std::size_t content_end   = close_pos;
    while (content_start < content_end
           && std::isspace(static_cast<unsigned char>(
                  source[content_start]))) {
        ++content_start;
    }

    block.found      = true;
    block.start_line = lineOfByte(source, content_start);
    block.content    = source.substr(content_start,
                                     content_end - content_start);
    block.lang       = extractAttribute(tag_attrs, "lang");

    // 7. Deteksi script block kedua (Vue spec: hanya satu).
    const std::size_t second_open =
        findCaseInsensitive(source, "<script", close_pos + 8);
    if (second_open != std::string::npos) {
        std::fprintf(stderr,
            "[frans][vue] warning: multiple <script> blocks — "
            "hanya blok pertama yang di-parse\n");
        std::fflush(stderr);
    }

    return block;
}

// ---------------------------------------------------------------------------
// selectParser
// ---------------------------------------------------------------------------
ILanguageParser* VueParser::selectParser(const std::string& lang) {
    if (lang == "ts")  return ts_parser_.get();
    if (lang == "tsx") return tsx_parser_.get();
    // "js", "javascript", "" → JS parser (Vue default).
    return js_parser_.get();
}

// ---------------------------------------------------------------------------
// parse
// ---------------------------------------------------------------------------
ParsedFile VueParser::parse(const std::string& file_path,
                             const std::string& source)
{
    ParsedFile out;
    out.file_path = file_path;

    const ScriptBlock block = findScriptBlock(source);
    if (!block.found) {
        return out;   // tidak ada <script> → empty ParsedFile
    }

    ILanguageParser* inner_parser = selectParser(block.lang);
    if (inner_parser == nullptr) {
        return out;   // defensive — tidak pernah terjadi
    }

    ParsedFile inner = inner_parser->parse(file_path, block.content);

    // ---- Line adjustment ----
    //
    // TS/JS parser melaporkan baris RELATIF terhadap content yang
    // kita berikan. Kita shift supaya menunjuk baris ASLI di .vue:
    //
    //   absolute = relative + block.start_line - 1
    //
    // Karena block.content sudah di-strip leading whitespace,
    // content line 1 == block.start_line, jadi offset konversinya
    // persis (block.start_line - 1).
    //
    // Relations & imports tidak punya field line di data model kita,
    // jadi tidak perlu adjustment. file_path sudah benar (diteruskan
    // dari argumen parse()).
    const int offset = block.start_line - 1;
    if (offset != 0) {
        for (auto& s : inner.snippets) {
            s.start_line += offset;
            s.end_line   += offset;
        }
    }

    return inner;
}

} // namespace frans::code_intelligence
