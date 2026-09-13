// ~/AgenAI/frans/src/code_intelligence/builder/TreeSitterGraphBuilder.h
#pragma once

#include "code_intelligence/graph/DependencyGraph.h"
#include "code_intelligence/parsers/PythonParser.h"

#include <string>
#include <vector>

namespace frans::code_intelligence {

// Blueprint §5.6 — IDependencyGraphBuilder (implementasi konkret).
//
// Orchestrator: scan project root → panggil PythonParser per file →
// update DependencyGraph. File yang gagal di-parse dicatat di errors_,
// TIDAK menghentikan build.
class TreeSitterGraphBuilder {
public:
    TreeSitterGraphBuilder() = default;

    // Scan project_root secara rekursif, cari semua file *.py, parse,
    // dan isi graph. Graph di-reset dulu (build = full scan).
    void build(const std::string& project_root);

    // Incremental update untuk satu file. File harus sudah ada di graph
    // (atau ditambah baru kalau belum pernah dilihat).
    void updateFile(const std::string& path);

    const DependencyGraph& graph() const { return graph_; }
    DependencyGraph&       graph()       { return graph_; }

    const std::vector<std::string>& errors() const { return errors_; }

private:
    DependencyGraph graph_;
    PythonParser    parser_;
    std::vector<std::string> errors_;

    // Helper: cari semua .py di bawah root (deterministik: hasil di-sort).
    std::vector<std::string> discoverPythonFiles(
        const std::string& root) const;

    // Baca file → parse → masukkan ke graph. Return false kalau gagal.
    bool parseAndAdd(const std::string& path);
};

} // namespace frans::code_intelligence
