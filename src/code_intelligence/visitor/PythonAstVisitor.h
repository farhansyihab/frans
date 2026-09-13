// ~/AgenAI/frans/src/code_intelligence/visitor/PythonAstVisitor.h
#pragma once

#include "code_intelligence/parsers/ParsedFile.h"
#include "code_intelligence/visitor/IAstVisitor.h"

#include <string>

// Forward decl tree-sitter (biar header ini tetap ringan).
struct TSNode;

namespace frans::code_intelligence {

// Implementasi Visitor untuk AST Python hasil tree-sitter.
//
// Alur:
//   PythonAstVisitor v(file_path, source);
//   v.walk(root_node);            // traverse tree-sitter, emit events
//   ParsedFile pf = v.takeResult();
//
// Catatan desain: visitor ini tahu tree-sitter, tapi tetap implement
// IAstVisitor secara terpisah supaya kalau nanti ada jalur visitor lain
// (mis. CppAstVisitor) yang tidak pakai tree-sitter, interface-nya tetap
// bersih.
class PythonAstVisitor : public IAstVisitor {
public:
    PythonAstVisitor(std::string file_path, const std::string& source);

    // Traverse tree-sitter AST mulai dari `root`. Setelah selesai,
    // ParsedFile siap diambil via takeResult().
    void walk(TSNode root);

    ParsedFile takeResult();

    // ---- IAstVisitor ----
    void onFunction(const FunctionInfo&) override;
    void onClass(const ClassInfo&) override;
    void onCall(const CallInfo&) override;
    void onImport(const ImportInfo&) override;

private:
    struct WalkState {
        std::string enclosing_function;  // "" kalau top-level
        std::string enclosing_class;     // "" kalau top-level
    };

    void visitNode(TSNode node, const WalkState& state);

    // Helpers ekstraksi (semua menerima TSNode + source sudah di-bind).
    void handleFunction(TSNode node, const WalkState& state);
    void handleClass(TSNode node, const WalkState& state);
    void handleCall(TSNode node, const WalkState& state);
    void handleImport(TSNode node);
    void handleImportFrom(TSNode node);

    std::string textOf(TSNode node) const;
    std::string attributeTailName(TSNode attribute_node) const;
    bool        isSelfAttribute(TSNode attribute_node) const;

    std::string file_path_;
    std::string source_;    // copy? tidak — referensi ke caller
    const std::string* source_ref_ = nullptr;
    ParsedFile  result_;
};

} // namespace frans::code_intelligence
