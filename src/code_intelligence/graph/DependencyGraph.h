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

    // Akses read-only ke indeks simbol. Map: symbol_name -> [(file, idx)]
    // dengan idx = posisi snippet di files_[file].snippets.
    //
    // Dipakai ContextSelector (Stage 6) untuk fuzzy match nama simbol dari
    // prompt user tanpa perlu copy seluruh snapshot. Caller TIDAK boleh
    // menyimpan reference ini melewati lifetime graph atau setelah mutasi
    // (addFile/removeFile/clear) — index di-rebuild.
    const std::map<
        std::string,
        std::vector<std::pair<std::string, std::size_t>>>&
    allSymbols() const { return symbol_index_; }

    // Stage 7g+1 (patch) — list semua file path yang ada di graph
    // (termasuk file yang tidak punya symbol/snippet sama sekali).
    // Hasil di-sort alfabetis (files_ adalah std::map, jadi iterasi
    // sudah sorted by key).
    //
    // Dipakai TargetScopeResolver untuk mencocokkan nama file yang
    // disebut eksplisit di task description dengan file yang benar-benar
    // ada di graph.
    std::vector<std::string> allFilePaths() const;

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
