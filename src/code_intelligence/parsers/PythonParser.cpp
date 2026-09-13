// ~/AgenAI/frans/src/code_intelligence/parsers/PythonParser.cpp
#include "code_intelligence/parsers/PythonParser.h"

#include "code_intelligence/visitor/PythonAstVisitor.h"

#include <tree_sitter/api.h>

#include <algorithm>

// Grammar Python dari tree-sitter-python — linkage C.
extern "C" const TSLanguage* tree_sitter_python(void);

namespace frans::code_intelligence {

PythonParser::PythonParser() {
    parser_ = ts_parser_new();
    if (parser_) {
        ts_parser_set_language(parser_, tree_sitter_python());
    }
}

PythonParser::~PythonParser() {
    if (parser_) {
        ts_parser_delete(parser_);
        parser_ = nullptr;
    }
}

ParsedFile PythonParser::parse(const std::string& file_path,
                               const std::string& source) {
    ParsedFile out;
    out.file_path = file_path;

    if (!parser_) return out;

    TSTree* tree = ts_parser_parse_string(
        parser_,
        /*old_tree=*/nullptr,
        source.data(),
        static_cast<uint32_t>(source.size()));

    if (!tree) return out;

    PythonAstVisitor visitor(file_path, source);
    visitor.walk(ts_tree_root_node(tree));
    out = visitor.takeResult();

    ts_tree_delete(tree);

    // Determinisme (blueprint catatan teknis sesi 5).
    std::sort(out.snippets.begin(), out.snippets.end());
    return out;
}

} // namespace frans::code_intelligence
