// ~/AgenAI/frans/src/code_intelligence/parsers/PythonParser.h
#pragma once

#include "code_intelligence/parsers/ILanguageParser.h"

// Forward decl tree-sitter types — jangan include api.h di header publik.
struct TSParser;

namespace frans::code_intelligence {

// Blueprint §5.6 — Strategy: ILanguageParser untuk bahasa Python.
//
// RAII: constructor menyiapkan TSParser, destructor membersihkan.
// Non-copyable (TSParser tidak copyable).
//
// Tidak thread-safe: satu instance = satu thread. Kalau nanti butuh
// parse paralel, buat instance terpisah per worker.
class PythonParser : public ILanguageParser {
public:
    PythonParser();
    ~PythonParser() override;

    PythonParser(const PythonParser&)            = delete;
    PythonParser& operator=(const PythonParser&) = delete;

    std::string language() const override { return "python"; }

    ParsedFile parse(const std::string& file_path,
                     const std::string& source) override;

private:
    TSParser* parser_ = nullptr;
};

} // namespace frans::code_intelligence
