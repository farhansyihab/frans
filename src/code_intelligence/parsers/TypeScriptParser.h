// ~/AgenAI/frans/src/code_intelligence/parsers/TypeScriptParser.h
#pragma once

#include "code_intelligence/parsers/ILanguageParser.h"
#include "code_intelligence/parsers/TsJsParserBase.h"

namespace frans::code_intelligence {

// Blueprint Stage 10b — ILanguageParser untuk TypeScript.
//
// Dua variant grammar:
//   - Variant::Ts  → .ts  (tree_sitter_typescript)
//   - Variant::Tsx → .tsx (tree_sitter_tsx)
//
// Beda karena di TSX karakter `<` ambigu: bisa generic parameter
// (`Array<T>`) atau elemen JSX (`<div>`). Grammar-nya beda, jadi
// caller WAJIB memilih variant yang tepat per file.
//
// RAII, non-copyable, tidak thread-safe (satu instance = satu thread).
class TypeScriptParser : public ILanguageParser,
                          private TsJsParserBase {
public:
    enum class Variant { Ts, Tsx };

    explicit TypeScriptParser(Variant variant = Variant::Ts);
    ~TypeScriptParser() override;

    TypeScriptParser(const TypeScriptParser&)            = delete;
    TypeScriptParser& operator=(const TypeScriptParser&) = delete;

    std::string language() const override { return "typescript"; }

    ParsedFile parse(const std::string& file_path,
                     const std::string& source) override;
};

} // namespace frans::code_intelligence
