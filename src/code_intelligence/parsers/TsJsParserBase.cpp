// ~/AgenAI/frans/src/code_intelligence/parsers/TsJsParserBase.cpp
#include "code_intelligence/parsers/TsJsParserBase.h"

#include "code_intelligence/visitor/TsJsAstVisitor.h"

#include <tree_sitter/api.h>

#include <algorithm>
#include <cstdio>

namespace frans::code_intelligence {

TsJsParserBase::TsJsParserBase(const TSLanguage* lang) {
    parser_ = ts_parser_new();
    if (parser_ && lang) {
        if (!ts_parser_set_language(parser_, lang)) {
            // Grammar ABI mismatch — jarang, tapi mungkin kalau grammar
            // di-clone dari commit yang sangat berbeda dari tree-sitter
            // core. Log & biarkan parser_ hidup (parse() akan return
            // ParsedFile kosong, tidak crash).
            std::fprintf(stderr,
                "[frans][tsjs] ts_parser_set_language() gagal — "
                "grammar ABI mungkin tidak cocok\n");
            std::fflush(stderr);
        }
    }
}

TsJsParserBase::~TsJsParserBase() {
    if (parser_) {
        ts_parser_delete(parser_);
        parser_ = nullptr;
    }
}

ParsedFile TsJsParserBase::parseImpl(const std::string& file_path,
                                      const std::string& source,
                                      const char* language_label) {
    ParsedFile out;
    out.file_path = file_path;

    (void)language_label;   // reserved untuk logging

    if (!parser_) return out;

    TSTree* tree = ts_parser_parse_string(
        parser_,
        /*old_tree=*/nullptr,
        source.data(),
        static_cast<uint32_t>(source.size()));

    if (!tree) return out;

    TsJsAstVisitor visitor(file_path, source);
    visitor.walk(ts_tree_root_node(tree));
    out = visitor.takeResult();

    ts_tree_delete(tree);

    // Determinisme — sama seperti PythonParser (blueprint §3.3 aturan
    // yang di-inherit dari Stage 5).
    std::sort(out.snippets.begin(), out.snippets.end());
    return out;
}

} // namespace frans::code_intelligence
