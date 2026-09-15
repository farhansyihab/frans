// ~/AgenAI/frans/src/code_intelligence/builder/TreeSitterGraphBuilder.cpp
#include "code_intelligence/builder/TreeSitterGraphBuilder.h"

// Python parser selalu tersedia kalau code_intel di-build.
#include "code_intelligence/parsers/PythonParser.h"

// Parser conditional — hanya include/register kalau grammar tersedia.
#ifdef FRANS_HAS_CPP_PARSER
#include "code_intelligence/parsers/CppParser.h"
#endif
#ifdef FRANS_HAS_TS_PARSER
#include "code_intelligence/parsers/TypeScriptParser.h"
#endif
#ifdef FRANS_HAS_JS_PARSER
#include "code_intelligence/parsers/JavaScriptParser.h"
#endif
#ifdef FRANS_HAS_VUE_PARSER
#include "code_intelligence/parsers/VueParser.h"
#endif

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace frans::code_intelligence {

namespace fs = std::filesystem;

namespace {

// Stage 10d — direktori yang di-skip saat discovery.
//
// Ini union dari:
//   - Stage 5 list: .git, venv, .venv, __pycache__, build, node_modules
//   - Stage 10d addendum: dist, .next, out, target, bin, obj
//     (build output umum untuk TS/JS (dist, .next, out),
//      Rust/Cargo (target), dan C/C++ build system (bin, obj))
//
// Trade-off documented di blueprint §7.9: user yang sengaja menaruh
// kode di folder bernama `build/` tidak akan ter-parse. Sangat jarang;
// user bisa rename folder kalau perlu.
bool isSkippedDir(const std::string& name) {
    return name == ".git"
        || name == "venv"  || name == ".venv"
        || name == "__pycache__"
        || name == "build"
        || name == "node_modules"
        || name == "dist"  || name == ".next" || name == "out"
        || name == "target"
        || name == "bin"   || name == "obj";
}

} // namespace

// ---------------------------------------------------------------------------
// supportedExtensions — Stage 10d
// ---------------------------------------------------------------------------
std::vector<std::string> TreeSitterGraphBuilder::supportedExtensions() {
    std::vector<std::string> out;
    out.reserve(14);

    // Python selalu ada (kalau frans_code_intel di-build).
    out.push_back(".py");

#ifdef FRANS_HAS_CPP_PARSER
    out.push_back(".h");
    out.push_back(".hpp");
    out.push_back(".hxx");
    out.push_back(".cc");
    out.push_back(".cpp");
    out.push_back(".cxx");
#endif

#ifdef FRANS_HAS_TS_PARSER
    out.push_back(".ts");
    out.push_back(".tsx");
#endif

#ifdef FRANS_HAS_JS_PARSER
    out.push_back(".js");
    out.push_back(".jsx");
    out.push_back(".mjs");
    out.push_back(".cjs");
#endif

#ifdef FRANS_HAS_VUE_PARSER
    out.push_back(".vue");
#endif

    return out;
}

// ---------------------------------------------------------------------------
// Construction / destruction — Stage 10d parser registry
// ---------------------------------------------------------------------------
TreeSitterGraphBuilder::TreeSitterGraphBuilder() {
    // Catatan: satu instance per ekstensi (bukan per bahasa).
    // Trade-off kecil (overhead instance) untuk keamanan future
    // parallel parsing. Lihat komentar di header.
    parsers_[".py"] = std::make_unique<PythonParser>();

#ifdef FRANS_HAS_CPP_PARSER
    parsers_[".h"]   = std::make_unique<CppParser>();
    parsers_[".hpp"] = std::make_unique<CppParser>();
    parsers_[".hxx"] = std::make_unique<CppParser>();
    parsers_[".cc"]  = std::make_unique<CppParser>();
    parsers_[".cpp"] = std::make_unique<CppParser>();
    parsers_[".cxx"] = std::make_unique<CppParser>();
#endif

#ifdef FRANS_HAS_TS_PARSER
    parsers_[".ts"]  = std::make_unique<TypeScriptParser>(
        TypeScriptParser::Variant::Ts);
    parsers_[".tsx"] = std::make_unique<TypeScriptParser>(
        TypeScriptParser::Variant::Tsx);
#endif

#ifdef FRANS_HAS_JS_PARSER
    parsers_[".js"]  = std::make_unique<JavaScriptParser>();
    parsers_[".jsx"] = std::make_unique<JavaScriptParser>();
    parsers_[".mjs"] = std::make_unique<JavaScriptParser>();
    parsers_[".cjs"] = std::make_unique<JavaScriptParser>();
#endif

#ifdef FRANS_HAS_VUE_PARSER
    parsers_[".vue"] = std::make_unique<VueParser>();
#endif
}

TreeSitterGraphBuilder::~TreeSitterGraphBuilder() = default;

// ---------------------------------------------------------------------------
// discoverSourceFiles — Stage 10d multi-extension
// ---------------------------------------------------------------------------
std::vector<std::string> TreeSitterGraphBuilder::discoverSourceFiles(
    const std::string& root) const
{
    std::vector<std::string> out;
    std::error_code ec;

    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return out;
    }

    // Pre-compute extension set dari registry (bukan hardcode) — supaya
    // kalau registry berubah, discovery otomatis ikut.
    std::set<std::string> known_exts;
    for (const auto& [ext, _] : parsers_) {
        known_exts.insert(ext);
    }

    for (auto it = fs::recursive_directory_iterator(
             root,
             fs::directory_options::skip_permission_denied,
             ec);
         it != fs::recursive_directory_iterator();
         it.increment(ec))
    {
        if (ec) break;
        const auto& p = it->path();

        const bool is_dir = fs::is_directory(p, ec);
        if (is_dir) {
            if (isSkippedDir(p.filename().string())) {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (known_exts.count(p.extension().string()) > 0) {
            out.push_back(p.string());
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

// ---------------------------------------------------------------------------
// parseAndAdd — Stage 10d: lookup parser via registry
// ---------------------------------------------------------------------------
bool TreeSitterGraphBuilder::parseAndAdd(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        errors_.push_back("cannot open: " + path);
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string src = ss.str();

    const std::string ext = fs::path(path).extension().string();
    auto it = parsers_.find(ext);
    if (it == parsers_.end()) {
        // Seharusnya tidak terjadi di flow normal (discoverSourceFiles
        // hanya return file dengan ext yang terdaftar). Bisa terjadi
        // kalau updateFile(path) dipanggil langsung dengan ext aneh.
        errors_.push_back("unsupported extension: " + ext
                          + " for " + path);
        return false;
    }

    ParsedFile pf = it->second->parse(path, src);

    // File valid tapi tanpa snippet tetap ditambahkan — berguna untuk
    // file kosong, .vue tanpa <script>, dll. Ini menjaga file_count
    // akurat (mencerminkan file yang "dilihat" builder, bukan hanya
    // file yang menghasilkan symbol).
    graph_.addFile(pf);
    return true;
}

// ---------------------------------------------------------------------------
// build — Stage 10d + log summary
// ---------------------------------------------------------------------------
void TreeSitterGraphBuilder::build(const std::string& project_root) {
    graph_.clear();
    errors_.clear();

    const auto files = discoverSourceFiles(project_root);

    // Count file per bahasa (untuk log summary). Instance parser punya
    // language() method — kita manfaatkan itu daripada hardcode map.
    std::map<std::string, std::size_t> lang_counts;

    for (const auto& f : files) {
        parseAndAdd(f);

        const std::string ext = fs::path(f).extension().string();
        auto it = parsers_.find(ext);
        if (it != parsers_.end()) {
            ++lang_counts[it->second->language()];
        }
    }

    // -------------------------------------------------------------------
    // Log summary — Stage 10d
    // -------------------------------------------------------------------
    std::string lang_summary;
    for (const auto& [lang, count] : lang_counts) {
        if (!lang_summary.empty()) lang_summary += ", ";
        lang_summary += lang + "=" + std::to_string(count);
    }

    std::fprintf(stderr,
        "[frans][builder] build complete: %zu files, %zu symbols, "
        "%zu edges (%zu languages)%s%s\n",
        graph_.fileCount(),
        graph_.symbolCount(),
        graph_.edgeCount(),
        lang_counts.size(),
        lang_summary.empty() ? "" : "  [",
        lang_summary.empty() ? "" : (lang_summary + "]").c_str());
    std::fflush(stderr);

    if (!errors_.empty()) {
        std::fprintf(stderr,
            "[frans][builder] %zu file gagal di-parse (lihat errors())\n",
            errors_.size());
        std::fflush(stderr);
    }
}

void TreeSitterGraphBuilder::updateFile(const std::string& path) {
    // File mungkin baru dihapus → hapus dari graph.
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        graph_.removeFile(path);
        return;
    }
    // removeFile dulu lalu addFile — supaya entry lama benar-benar
    // hilang sebelum di-replace (incremental update pattern).
    graph_.removeFile(path);
    parseAndAdd(path);
}

} // namespace frans::code_intelligence
