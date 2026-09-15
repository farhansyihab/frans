// ~/AgenAI/frans/src/code_intelligence/builder/TreeSitterGraphBuilder.h
#pragma once

#include "code_intelligence/graph/DependencyGraph.h"
#include "code_intelligence/parsers/ILanguageParser.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace frans::code_intelligence {

// Blueprint §5.6 (IDependencyGraphBuilder) + Stage 10d (multi-language).
//
// Orchestrator: scan project root → pilih parser per ekstensi (via
// registry) → update DependencyGraph. File yang gagal di-parse dicatat
// di errors_, TIDAK menghentikan build.
//
// Stage 10d perubahan:
//   - Parser registry (std::map<ext, unique_ptr<ILanguageParser>>)
//     menggantikan member `PythonParser parser_` tunggal.
//   - `discoverPythonFiles` → `discoverSourceFiles` (multi-ekstensi,
//     berdasarkan registry — bukan list hardcode).
//   - Static `supportedExtensions()` di-expose supaya
//     ProjectGraphCache::discoverSourceFiles() bisa pakai list yang
//     SAMA (mencegah drift antara builder & cache).
//   - Log summary "build complete: N files across M languages".
//
// Stage 10d conditional compilation: parser untuk bahasa tertentu
// hanya di-register kalau macro `FRANS_HAS_<LANG>_PARSER` di-define
// oleh CMake. Ini mencegah link error kalau grammar tidak di-clone.
class TreeSitterGraphBuilder {
public:
    TreeSitterGraphBuilder();
    ~TreeSitterGraphBuilder();

    TreeSitterGraphBuilder(const TreeSitterGraphBuilder&)            = delete;
    TreeSitterGraphBuilder& operator=(const TreeSitterGraphBuilder&) = delete;

    // Scan project_root secara rekursif, cari semua file dengan ekstensi
    // yang didukung, parse, dan isi graph. Graph di-reset dulu
    // (build = full scan).
    void build(const std::string& project_root);

    // Incremental update untuk satu file. File harus sudah ada di graph
    // (atau ditambah baru kalau belum pernah dilihat).
    void updateFile(const std::string& path);

    const DependencyGraph& graph() const { return graph_; }
    DependencyGraph&       graph()       { return graph_; }

    const std::vector<std::string>& errors() const { return errors_; }

    // Stage 10d — list ekstensi yang didukung oleh build ini,
    // berdasarkan macro compile-time yang di-set CMake.
    //
    // Dipakai ProjectGraphCache::discoverSourceFiles() supaya discovery
    // untuk rebuild-trigger menggunakan list yang SAMA dengan yang
    // benar-benar di-parse builder. Kalau bahasa tertentu di-skip
    // (grammar tidak ada), cache juga tidak akan "melihat" file
    // bahasa tersebut — konsisten.
    //
    // Return list TERURUT (deterministik). Contoh: {".py", ".h",
    // ".hpp", ".hxx", ".cc", ".cpp", ".cxx", ".ts", ".tsx", ".js",
    // ".jsx", ".mjs", ".cjs", ".vue"}.
    static std::vector<std::string> supportedExtensions();

private:
    DependencyGraph graph_;
    std::vector<std::string> errors_;

    // Registry: extension → parser instance.
    //
    // Satu instance per ekstensi (BUKAN satu per bahasa). Alasannya:
    // TSParser* tidak thread-safe — kalau nanti parsing di-paralel,
    // tiap worker harus punya instance sendiri. Sekarang parsing
    // sequential, jadi overhead instance tambahan ~microseconds.
    std::map<std::string, std::unique_ptr<ILanguageParser>> parsers_;

    // Helper: cari semua file dengan ekstensi yang didukung di bawah
    // root (deterministik: hasil di-sort). Skip directory yang jelas
    // tidak relevan (lihat kSkipDirs di .cpp).
    std::vector<std::string> discoverSourceFiles(
        const std::string& root) const;

    // Baca file → parse → masukkan ke graph. Return false kalau gagal
    // (I/O error atau ekstensi tidak terdaftar di registry).
    bool parseAndAdd(const std::string& path);
};

} // namespace frans::code_intelligence
