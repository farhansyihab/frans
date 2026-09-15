// ~/AgenAI/frans/src/code_intelligence/parsers/CppParser.h
#pragma once

#include "code_intelligence/parsers/ILanguageParser.h"

// Forward decl tree-sitter types — jangan include api.h di header publik.
struct TSParser;

namespace frans::code_intelligence {

// Blueprint §10a — Strategy: ILanguageParser untuk bahasa C++.
//
// RAII: constructor menyiapkan TSParser dengan grammar tree-sitter-cpp,
// destructor membersihkan. Non-copyable.
//
// Tidak thread-safe: satu instance = satu thread. Kalau nanti butuh
// parse paralel, buat instance terpisah per worker (lihat catatan
// blueprint §7.11 soal TSParser tidak thread-safe).
class CppParser : public ILanguageParser {
public:
    CppParser();
    ~CppParser() override;

    CppParser(const CppParser&)            = delete;
    CppParser& operator=(const CppParser&) = delete;

    std::string language() const override { return "cpp"; }

    // `source` = isi file C++ (sudah dibaca caller).
    // Return ParsedFile dengan snippets sudah di-sort deterministik
    // (by file_path, start_line, symbol_name).
    //
    // TIDAK membuka file sendiri — parser murni (blueprint §2 prinsip 4).
    ParsedFile parse(const std::string& file_path,
                     const std::string& source) override;

private:
    TSParser* parser_ = nullptr;
};

} // namespace frans::code_intelligence
