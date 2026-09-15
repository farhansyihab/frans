// ~/AgenAI/frans/src/code_intelligence/parsers/JavaScriptParser.cpp
#include "code_intelligence/parsers/JavaScriptParser.h"

#include <tree_sitter/api.h>

extern "C" const TSLanguage* tree_sitter_javascript(void);

namespace frans::code_intelligence {

JavaScriptParser::JavaScriptParser()
    : TsJsParserBase(tree_sitter_javascript())
{}

JavaScriptParser::~JavaScriptParser() = default;

ParsedFile JavaScriptParser::parse(const std::string& file_path,
                                    const std::string& source) {
    return parseImpl(file_path, source, "javascript");
}

} // namespace frans::code_intelligence
