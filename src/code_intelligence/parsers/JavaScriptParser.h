// ~/AgenAI/frans/src/code_intelligence/parsers/JavaScriptParser.h
#pragma once

#include "code_intelligence/parsers/ILanguageParser.h"
#include "code_intelligence/parsers/TsJsParserBase.h"

namespace frans::code_intelligence {

// Blueprint Stage 10b — ILanguageParser untuk JavaScript.
//
// Menggunakan grammar tree-sitter-javascript (yang juga support JSX).
// Digunakan untuk .js, .jsx, .mjs, .cjs — semuanya grammar yang sama,
// karena JS modern (ESM + JSX) all covered oleh grammar ini.
//
// RAII, non-copyable, tidak thread-safe.
class JavaScriptParser : public ILanguageParser,
                          private TsJsParserBase {
public:
    JavaScriptParser();
    ~JavaScriptParser() override;

    JavaScriptParser(const JavaScriptParser&)            = delete;
    JavaScriptParser& operator=(const JavaScriptParser&) = delete;

    std::string language() const override { return "javascript"; }

    ParsedFile parse(const std::string& file_path,
                     const std::string& source) override;
};

} // namespace frans::code_intelligence
