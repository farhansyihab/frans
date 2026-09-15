// ~/AgenAI/frans/src/code_intelligence/visitor/CppAstVisitor.h
#pragma once

#include "code_intelligence/parsers/ParsedFile.h"
#include "code_intelligence/visitor/IAstVisitor.h"

#include <string>
#include <vector>

// Forward decl tree-sitter (biar header ini tetap ringan).
struct TSNode;

namespace frans::code_intelligence {

// Implementasi Visitor untuk AST C++ hasil tree-sitter-cpp.
//
// Alur:
//   CppAstVisitor v(file_path, source);
//   v.walk(root_node);
//   ParsedFile pf = v.takeResult();
//
// Tiga level scope C++ yang di-track:
//   - namespace_scope ("ns1::ns2")     — accumulate saat masuk namespace_definition
//   - enclosing_class ("Foo::Inner")   — accumulate saat masuk class/struct body
//   - enclosing_function ("Foo::bar")  — symbol pemilik call-expression
//
// Pemisah symbol: "::" (konvensi C++), berbeda dari "." (TS/JS) — lihat
// blueprint §7.2.
class CppAstVisitor : public IAstVisitor {
public:
    CppAstVisitor(std::string file_path, const std::string& source);

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
    // State saat menelusuri AST. Di-copy saat masuk scope baru (murah —
    // string kecil, dan traversal rekursif jadi tidak ada aliasing).
    struct WalkState {
        std::string namespace_scope;      // "ns1::ns2" atau ""
        std::string enclosing_class;      // "Foo" | "Outer::Inner" | ""
        std::string enclosing_function;   // "Foo::bar" | "free_fn" | ""
    };

    void visitNode(TSNode node, const WalkState& state);

    // Node handlers — dipanggil dari visitNode berdasarkan ts_node_type().
    void handleClassSpecifier(TSNode node, const WalkState& state);
    void handleStructSpecifier(TSNode node, const WalkState& state);
    void handleFunctionDefinition(TSNode node, const WalkState& state);
    void handleDeclaration(TSNode node, const WalkState& state);
    void handleNamespaceDefinition(TSNode node, const WalkState& state);
    void handlePreprocInclude(TSNode node);
    void handleCallExpression(TSNode node, const WalkState& state);

    // Helpers
    std::string textOf(TSNode node) const;

    // Bangun fully-qualified name dari state + leaf name.
    // Aturan: [ns::][class::]name (bagian kosong di-skip).
    std::string qualify(const WalkState& state,
                        const std::string& name) const;

    // Tentukan parent_class untuk sebuah FunctionInfo.
    //
    //   - Kalau `state.enclosing_class` non-empty (di dalam class body),
    //     langsung pakai itu.
    //   - Kalau kosong tapi `inner_decl` adalah `qualified_identifier`
    //     (mis. `double Shape::area()` di impl file), method ini "out-of-line"
    //     — turunkan parent dari `symbol_name` (text sebelum `::` terakhir).
    //   - Selain itu (simple identifier di namespace scope), kembalikan "".
    //
    // Return "" untuk namespace-scoped free function → kind="function".
    // Return non-empty untuk method → kind="method".
    std::string inferParentClass(TSNode inner_decl,
                                 const WalkState& state,
                                 const std::string& symbol_name) const;

    // Ambil base class names dari `base_class_clause`.
    // Mis. "public Base" → {"Base"}; "virtual protected Base, Other" → {"Base","Other"}.
    std::vector<std::string> extractBaseClassNames(TSNode base_clause) const;

    // Ambil call target name dari node "function" di call_expression:
    //   foo()            → "foo"
    //   obj.foo()        → "foo"
    //   obj->foo()       → "foo"
    //   this->foo()      → "foo"
    //   ns::foo()        → "foo"       (leaf property)
    //   Foo<int>::foo()  → "foo"       (template — leaf)
    std::string extractCallName(TSNode call_fn_node) const;

    // Ambil leaf name dari declarator (identifier / field_identifier /
    // destructor_name / operator_name / qualified_identifier). Untuk
    // qualified_identifier, kembalikan text lengkap (sudah qualified).
    // Caller yang memutuskan perlu qualify() tambahan atau tidak.
    std::string declaratorName(TSNode declarator, const WalkState& state) const;

    std::string file_path_;
    std::string source_;
    const std::string* source_ref_ = nullptr;
    ParsedFile  result_;
};

} // namespace frans::code_intelligence
