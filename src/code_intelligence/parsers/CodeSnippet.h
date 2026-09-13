// ~/AgenAI/frans/src/code_intelligence/parsers/CodeSnippet.h
#pragma once

#include <string>

namespace frans::code_intelligence {

// Blueprint §5.6 — granularitas per-fungsi, bukan per-file.
//
// Satu snippet mewakili satu simbol yang bisa dipilih ContextSelector
// sebagai konteks untuk model. `content` WAJIB full body (dari `def`
// sampai baris terakhir), bukan cuma signature.
struct CodeSnippet {
    std::string file_path;
    std::string symbol_name;
    std::string kind;           // "function" | "class" | "method"
    std::string parent_class;   // non-empty kalau kind == "method"
    int         start_line = 0; // 1-based, baris `def`/`class`
    int         end_line   = 0; // 1-based, inklusif
    std::string signature;      // baris pertama saja (def ...:)
    std::string content;        // full block

    // Sort deterministik — wajib sebelum return dari manapun.
    bool operator<(const CodeSnippet& o) const {
        if (file_path   != o.file_path)   return file_path   < o.file_path;
        if (start_line  != o.start_line)  return start_line  < o.start_line;
        return symbol_name < o.symbol_name;
    }
};

} // namespace frans::code_intelligence
