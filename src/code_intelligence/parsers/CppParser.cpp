// ~/AgenAI/frans/src/code_intelligence/parsers/CppParser.cpp
#include "code_intelligence/parsers/CppParser.h"

#include "code_intelligence/visitor/CppAstVisitor.h"

#include <tree_sitter/api.h>

#include <algorithm>

// Grammar C++ dari tree-sitter-cpp — linkage C.
// Scanner di-compile dari .cc (C++), tapi symbol di-export sebagai C.
extern "C" const TSLanguage* tree_sitter_cpp(void);

namespace frans::code_intelligence {

CppParser::CppParser() {
    parser_ = ts_parser_new();
    if (parser_) {
        ts_parser_set_language(parser_, tree_sitter_cpp());
    }
}

CppParser::~CppParser() {
    if (parser_) {
        ts_parser_delete(parser_);
        parser_ = nullptr;
    }
}

ParsedFile CppParser::parse(const std::string& file_path,
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

    CppAstVisitor visitor(file_path, source);
    visitor.walk(ts_tree_root_node(tree));
    out = visitor.takeResult();

    ts_tree_delete(tree);

    // Determinisme (blueprint §10a.7 assertion #9).
    std::sort(out.snippets.begin(), out.snippets.end());
    return out;
}

} // namespace frans::code_intelligence
