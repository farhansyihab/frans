// ~/AgenAI/frans/src/code_intelligence/parsers/ParsedFile.h
#pragma once

#include "code_intelligence/parsers/CodeSnippet.h"

#include <string>
#include <vector>

namespace frans::code_intelligence {

// Satu relasi simbol → simbol, hasil ekstraksi AST.
//   kind "calls"    : from_symbol memanggil to_symbol
//   kind "inherits" : from_symbol mewarisi to_symbol
struct SymbolRelation {
    std::string from_symbol;
    std::string to_symbol;
    std::string kind;
    std::string file_path;   // tempat relasi ditemukan
};

// Hasil parsing satu file. Dipakai TreeSitterGraphBuilder untuk
// meng-update DependencyGraph.
struct ParsedFile {
    std::string file_path;
    std::vector<CodeSnippet>    snippets;   // fungsi, class, method
    std::vector<std::string>    imports;    // nama modul / simbol
    std::vector<SymbolRelation> relations;  // calls + inherits
};

} // namespace frans::code_intelligence
