// ~/AgenAI/frans/src/code_intelligence/parsers/TypeScriptParser.cpp
#include "code_intelligence/parsers/TypeScriptParser.h"

#include <tree_sitter/api.h>

// Grammar TypeScript — linkage C dari tree-sitter-typescript.
// Dua entry point (repo ini punya dua subdirektori grammar):
//   tree_sitter_typescript() → untuk .ts
//   tree_sitter_tsx()        → untuk .tsx (JSX dalam TS)
extern "C" const TSLanguage* tree_sitter_typescript(void);
extern "C" const TSLanguage* tree_sitter_tsx(void);

namespace frans::code_intelligence {

TypeScriptParser::TypeScriptParser(Variant variant)
    : TsJsParserBase(
          variant == Variant::Tsx
              ? tree_sitter_tsx()
              : tree_sitter_typescript())
{}

TypeScriptParser::~TypeScriptParser() = default;

ParsedFile TypeScriptParser::parse(const std::string& file_path,
                                    const std::string& source) {
    return parseImpl(file_path, source, "typescript");
}

} // namespace frans::code_intelligence
