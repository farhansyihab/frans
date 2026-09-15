// ~/AgenAI/frans/src/code_intelligence/visitor/TsJsAstVisitor.h
#pragma once

#include "code_intelligence/parsers/ParsedFile.h"
#include "code_intelligence/visitor/IAstVisitor.h"

#include <string>

// Forward decl tree-sitter (biar header ini tetap ringan).
struct TSNode;

namespace frans::code_intelligence {

// Blueprint Stage 10b — Visitor untuk AST TypeScript / TSX / JavaScript.
//
// Satu visitor dipakai untuk ketiga grammar. Node-type yang di-handle
// identik (function_declaration, class_declaration, method_definition,
// dst.); perbedaan TS vs JS (type annotation, interface) tidak
// mempengaruhi struktur class/function/method yang kita ekstrak.
//
// Konvensi penamaan:
//   - Top-level function: "foo"
//   - Method di dalam class: "Foo.bar"  (dot, bukan "::")
//   - Nested function: "outer.inner"
//
// Alur:
//   TsJsAstVisitor v(file_path, source);
//   v.walk(root_node);
//   ParsedFile pf = v.takeResult();
class TsJsAstVisitor : public IAstVisitor {
public:
    TsJsAstVisitor(std::string file_path, const std::string& source);

    void walk(TSNode root);
    ParsedFile takeResult();

    // ---- IAstVisitor ----
    void onFunction(const FunctionInfo&) override;
    void onClass(const ClassInfo&) override;
    void onCall(const CallInfo&) override;
    void onImport(const ImportInfo&) override;

private:
    struct WalkState {
        // "Foo" kalau di dalam class, "" kalau top-level.
        // Nested class di-qualify, mis. "Outer.Inner".
        std::string enclosing_class;

        // Fully-qualified function name, e.g. "outer" atau "Foo.bar".
        // "" kalau belum di dalam function apapun. Di-set saat masuk
        // body function/method, dan di-reset saat masuk body class.
        std::string enclosing_function;
    };

    void visitNode(TSNode node, const WalkState& state);

    // Handlers per node type.
    void handleFunctionDeclaration(TSNode node, const WalkState& state);
    void handleVariableDeclaration(TSNode node, const WalkState& state);
    void handleClassDeclaration(TSNode node, const WalkState& state);
    void handleMethodDefinition(TSNode node, const WalkState& state);
    void handleImportStatement(TSNode node);
    void handleCallExpression(TSNode node, const WalkState& state);
    void handleNewExpression(TSNode node, const WalkState& state);

    // Helpers.
    std::string textOf(TSNode node) const;

    // Ambil module specifier dari string literal:
    //   "./path"  → "./path"   (quotes di-strip)
    // Return raw text kalau bukan string literal.
    std::string extractModuleSpecifier(TSNode string_node) const;

    // Ambil call target name:
    //   foo()            → "foo"
    //   obj.foo()        → "foo"
    //   obj.nested.foo() → "foo"
    //   this.foo()       → "foo"
    // Return "" kalau node bukan call target yang di-handle.
    std::string extractCallName(TSNode call_expr) const;

    std::string file_path_;
    std::string source_;
    ParsedFile  result_;
};

} // namespace frans::code_intelligence
