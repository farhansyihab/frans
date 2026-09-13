// ~/AgenAI/frans/src/code_intelligence/graph/DependencyGraph.h
#pragma once

#include "code_intelligence/parsers/ParsedFile.h"

#include <cstddef>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace frans::code_intelligence {

// Blueprint §5.6 — DependencyGraph.
//
// Menyimpan hasil parsing semua file, membangun indeks simbol dan edge
// antar simbol, lalu menyediakan traversal BFS `getRelatedSymbols()`.
//
// Keputusan desain: setiap addFile/removeFile memicu rebuild index penuh.
// Untuk project personal (<= beberapa ratus file) ini trivial; memilih
// incremental benar-benar hanya menambah kompleksitas tanpa manfaat nyata.
class DependencyGraph {
public:
    DependencyGraph() = default;

    // Buang semua state.
    void clear();

    // Tambah / ganti file di graph. Kalau file_path sudah ada, entri lama
    // dibuang dulu (incremental update path).
    void addFile(const ParsedFile& parsed);

    // Hapus file dari graph (dipakai kalau file dihapus dari project).
    void removeFile(const std::string& file_path);

    // Semua snippet yang symbol_name-nya sama persis (case-sensitive).
    std::vector<CodeSnippet> findBySymbol(const std::string& name) const;

    // BFS: kumpulkan semua snippet yang bisa dijangkau dari `name`
    // dalam <= maxHops edge (self tidak termasuk).
    std::vector<CodeSnippet> getRelatedSymbols(const std::string& name,
                                               int maxHops = 2) const;

    // Statistik untuk debug / UI.
    std::size_t fileCount()   const { return files_.size(); }
    std::size_t symbolCount() const { return symbol_index_.size(); }
    std::size_t edgeCount()   const;

    // Dump ringkas (untuk smoke test / terminal).
    void dumpTo(std::ostream& os) const;

private:
    void rebuildIndex();

    std::map<std::string, ParsedFile> files_;

    // symbol_name → list of (file_path, snippet_index di files_[file].snippets)
    std::map<std::string, std::vector<std::pair<std::string, std::size_t>>>
        symbol_index_;

    // symbol_name → set of outgoing symbol names
    std::map<std::string, std::set<std::string>> edges_;
};

} // namespace frans::code_intelligence
