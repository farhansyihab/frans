// ~/AgenAI/frans/src/code_intelligence/parsers/TsJsParserBase.h
#pragma once

#include "code_intelligence/parsers/ParsedFile.h"

#include <string>

// Forward-declare tree-sitter opaque types. Full definitions come from
// <tree_sitter/api.h>, included only in the .cpp.
struct TSParser;
struct TSLanguage;

namespace frans::code_intelligence {

// Blueprint Stage 10b — internal base class untuk TypeScriptParser dan
// JavaScriptParser.
//
// Kedua parser publik share ~90% logic: RAII TSParser, pipeline parse
// (ts_parser_parse_string → visitor → takeResult → sort), dan
// determinisme. Base ini menyediakan bagian tersebut; masing-masing
// parser publik tetap mengimplementasi ILanguageParser supaya punya
// identitas bahasa yang jelas (language() → "typescript" vs
// "javascript"), yang dipakai caller (TreeSitterGraphBuilder) untuk
// logging/statistik.
//
// Caller TIDAK pernah pakai base ini langsung — hanya via
// TypeScriptParser / JavaScriptParser.
class TsJsParserBase {
protected:
    // `lang` = pointer ke TSLanguage yang di-resolve dari grammar
    // (tree_sitter_typescript / tree_sitter_tsx / tree_sitter_javascript).
    // Base TIDAK memiliki lang pointer — caller yang menjamin hidup;
    // tree-sitter language objects adalah static data yang di-return
    // oleh fungsi extern "C" tree_sitter_*().
    explicit TsJsParserBase(const TSLanguage* lang);
    ~TsJsParserBase();

    TsJsParserBase(const TsJsParserBase&)            = delete;
    TsJsParserBase& operator=(const TsJsParserBase&) = delete;

    // Shared parse logic. `language_label` cuma untuk future logging,
    // tidak mempengaruhi output.
    ParsedFile parseImpl(const std::string& file_path,
                         const std::string& source,
                         const char* language_label);

    TSParser* parser_ = nullptr;
};

} // namespace frans::code_intelligence
