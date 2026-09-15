// ~/AgenAI/frans/src/code_intelligence/visitor/CppAstVisitor.cpp
#include "code_intelligence/visitor/CppAstVisitor.h"

#include <tree_sitter/api.h>

#include <cstring>
#include <utility>

namespace frans::code_intelligence {

namespace {

std::string toStr(TSNode node, const std::string& src) {
    const uint32_t s = ts_node_start_byte(node);
    const uint32_t e = ts_node_end_byte(node);
    if (s > e || e > src.size()) return {};
    return src.substr(s, e - s);
}

int startLine(TSNode node) {
    return static_cast<int>(ts_node_start_point(node).row) + 1;
}

int endLine(TSNode node) {
    return static_cast<int>(ts_node_end_point(node).row) + 1;
}

std::string firstLine(const std::string& s) {
    const auto p = s.find('\n');
    return p == std::string::npos ? s : s.substr(0, p);
}

TSNode childByField(TSNode node, const char* field) {
    return ts_node_child_by_field_name(
        node, field, static_cast<uint32_t>(std::strlen(field)));
}

// Buang quote/bracket dari string literal C++:
//   `"foo.hpp"`   → "foo.hpp"
//   `<vector>`    → "vector"
//   `<sys/types>` → "sys/types"
std::string stripIncludeDelims(const std::string& raw) {
    if (raw.size() < 2) return raw;
    const char f = raw.front();
    const char l = raw.back();
    if ((f == '"' && l == '"') || (f == '<' && l == '>')) {
        return raw.substr(1, raw.size() - 2);
    }
    return raw;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
CppAstVisitor::CppAstVisitor(std::string file_path,
                              const std::string& source)
    : file_path_(std::move(file_path)), source_(source) {
    source_ref_       = &source_;
    result_.file_path = file_path_;
}

ParsedFile CppAstVisitor::takeResult() {
    return std::move(result_);
}

std::string CppAstVisitor::textOf(TSNode node) const {
    return toStr(node, *source_ref_);
}

// ---------------------------------------------------------------------------
// IAstVisitor accumulation
// ---------------------------------------------------------------------------
void CppAstVisitor::onFunction(const FunctionInfo& fi) {
    CodeSnippet s;
    s.file_path    = file_path_;
    s.symbol_name  = fi.name;
    s.kind         = fi.parent_class.empty() ? "function" : "method";
    s.parent_class = fi.parent_class;
    s.start_line   = fi.start_line;
    s.end_line     = fi.end_line;
    s.signature    = fi.signature;
    s.content      = fi.content;
    result_.snippets.push_back(std::move(s));
}

void CppAstVisitor::onClass(const ClassInfo& ci) {
    CodeSnippet s;
    s.file_path   = file_path_;
    s.symbol_name = ci.name;
    s.kind        = "class";
    s.start_line  = ci.start_line;
    s.end_line    = ci.end_line;
    s.signature   = ci.signature;
    s.content     = ci.content;
    result_.snippets.push_back(std::move(s));

    for (const auto& base : ci.bases) {
        SymbolRelation r;
        r.from_symbol = ci.name;
        r.to_symbol   = base;
        r.kind        = "inherits";
        r.file_path   = file_path_;
        result_.relations.push_back(std::move(r));
    }
}

void CppAstVisitor::onCall(const CallInfo& info) {
    if (info.caller_symbol.empty() || info.callee_name.empty()) return;
    SymbolRelation r;
    r.from_symbol = info.caller_symbol;
    r.to_symbol   = info.callee_name;
    r.kind        = "calls";
    r.file_path   = file_path_;
    result_.relations.push_back(std::move(r));
}

void CppAstVisitor::onImport(const ImportInfo& info) {
    if (info.module.empty()) return;
    result_.imports.push_back(info.module);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
std::string CppAstVisitor::qualify(const WalkState& state,
                                    const std::string& name) const {
    if (name.empty()) return {};
    std::string out;
    if (!state.namespace_scope.empty()) {
        out += state.namespace_scope;
        out += "::";
    }
    if (!state.enclosing_class.empty()) {
        out += state.enclosing_class;
        out += "::";
    }
    out += name;
    return out;
}

std::string CppAstVisitor::inferParentClass(TSNode inner_decl,
                                             const WalkState& state,
                                             const std::string& symbol_name) const {
    // 1. Di dalam class body → parent_class = enclosing_class.
    if (!state.enclosing_class.empty()) {
        return state.enclosing_class;
    }

    // 2. Tidak di dalam class body, tapi declarator qualified
    //    (mis. `double Shape::area()` di .cpp) → ini method definition
    //    out-of-line. Turunkan parent dari symbol_name.
    //
    //    Catatan: kita bedakan `Shape::area` (method) dari
    //    `ns::free_function` (namespace-scoped function) dengan
    //    melihat declarator type. Kalau source menulis
    //    `void free_function()` di dalam `namespace ns {}`, declarator-nya
    //    adalah `identifier` biasa (bukan qualified_identifier), jadi
    //    kita TIDAK salah kategorikan sebagai method.
    //
    //    Corner case (acceptable limitation): kalau source menulis
    //    `void ns::free_function() {}` secara eksplisit di luar
    //    namespace, declarator qualified → kita klasifikasikan sebagai
    //    method (kind="method", parent_class="ns"). Secara semantik
    //    salah, tapi secara sintaks tidak bisa dibedakan tanpa symbol
    //    table. Kasus ini sangat jarang, dan symbol_name tetap benar
    //    sehingga ContextSelector tetap bisa menemukan function-nya.
    if (ts_node_is_null(inner_decl)) return {};
    if (std::strcmp(ts_node_type(inner_decl), "qualified_identifier") != 0) {
        return {};
    }

    const auto pos = symbol_name.rfind("::");
    if (pos == std::string::npos) return {};
    return symbol_name.substr(0, pos);
}

std::vector<std::string>
CppAstVisitor::extractBaseClassNames(TSNode base_clause) const {
    std::vector<std::string> out;
    const uint32_t nc = ts_node_named_child_count(base_clause);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode child = ts_node_named_child(base_clause, i);
        const char* t = ts_node_type(child);
        // access_specifier (public/protected/private) dan keyword
        // `virtual` bukan named node — jadi tidak akan muncul di sini.
        if (std::strcmp(t, "type_identifier")      == 0
         || std::strcmp(t, "qualified_identifier") == 0
         || std::strcmp(t, "template_type")        == 0) {
            std::string txt = textOf(child);
            if (!txt.empty()) out.push_back(std::move(txt));
        }
    }
    return out;
}

std::string CppAstVisitor::extractCallName(TSNode call_fn) const {
    if (ts_node_is_null(call_fn)) return {};
    const char* t = ts_node_type(call_fn);

    // foo()  |  Foo()
    if (std::strcmp(t, "identifier") == 0) {
        return textOf(call_fn);
    }

    // obj.method()  |  obj->method()  |  this->method()
    if (std::strcmp(t, "field_expression") == 0) {
        const TSNode field = childByField(call_fn, "field");
        if (!ts_node_is_null(field)) return textOf(field);
        return {};
    }

    // ns::foo()  → leaf name = "foo"
    if (std::strcmp(t, "qualified_identifier") == 0) {
        const TSNode name = childByField(call_fn, "name");
        if (!ts_node_is_null(name)) return textOf(name);
        // Fallback: ambil token terakhir dari text.
        const std::string txt = textOf(call_fn);
        const auto pos = txt.rfind("::");
        return (pos == std::string::npos) ? txt : txt.substr(pos + 2);
    }

    // foo<int>()  → name field = "foo"
    if (std::strcmp(t, "template_function") == 0
     || std::strcmp(t, "template_method")   == 0) {
        const TSNode name = childByField(call_fn, "name");
        if (!ts_node_is_null(name)) return textOf(name);
        return {};
    }

    // Parenthesized / lambda-invoke / subscript — skip.
    return {};
}

std::string CppAstVisitor::declaratorName(TSNode declarator,
                                            const WalkState& state) const {
    if (ts_node_is_null(declarator)) return {};
    const char* t = ts_node_type(declarator);

    // Sudah fully-qualified di source (mis. `void Foo::bar() {}`).
    // Prepend namespace saja; JANGAN prepend enclosing_class karena
    // source sudah menyebutkannya — kalau kita prepend juga jadi dobel.
    if (std::strcmp(t, "qualified_identifier") == 0) {
        const std::string txt = textOf(declarator);
        if (txt.empty()) return {};
        if (state.namespace_scope.empty()) return txt;
        // qualified_identifier sudah mengandung "::", langsung prepend ns.
        return state.namespace_scope + "::" + txt;
    }

    // Identifier sederhana: destructor_name (~Foo), operator_name
    // (operator+), field_identifier, type_identifier.
    const std::string leaf = textOf(declarator);
    if (leaf.empty()) return {};
    return qualify(state, leaf);
}

// ---------------------------------------------------------------------------
// Traversal
// ---------------------------------------------------------------------------
void CppAstVisitor::walk(TSNode root) {
    WalkState s;
    visitNode(root, s);
}

void CppAstVisitor::visitNode(TSNode node, const WalkState& state) {
    if (ts_node_is_null(node)) return;

    const char* type = ts_node_type(node);

    if (std::strcmp(type, "class_specifier") == 0) {
        handleClassSpecifier(node, state);
        return;   // handler menelusuri body sendiri dengan state baru.
    }
    if (std::strcmp(type, "struct_specifier") == 0) {
        handleStructSpecifier(node, state);
        return;
    }
    if (std::strcmp(type, "function_definition") == 0) {
        handleFunctionDefinition(node, state);
        return;
    }
    if (std::strcmp(type, "namespace_definition") == 0) {
        handleNamespaceDefinition(node, state);
        return;
    }
    if (std::strcmp(type, "preproc_include") == 0) {
        handlePreprocInclude(node);
        return;
    }

    // Deklarasi fungsi (punya function_declarator, tanpa body):
    //   - `field_declaration` — di dalam class body (method signature).
    //   - `declaration`       — top-level / namespace scope (function
    //                           prototype, mis. `void free_function();`).
    // Keduanya di-dispatch, tapi handleDeclaration() yang memfilter
    // (skip kalau sedang di dalam function body).
    if (std::strcmp(type, "field_declaration") == 0
     || std::strcmp(type, "declaration")       == 0) {
        handleDeclaration(node, state);
        // Fall through — body mungkin mengandung call_expression.
    }

    if (std::strcmp(type, "call_expression") == 0) {
        handleCallExpression(node, state);
        // Fall through — nested call f(g()) tetap perlu ditelusuri.
    }

    // Recurse ke semua named children dengan state yang sama.
    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        visitNode(ts_node_named_child(node, i), state);
    }
}

// ---------------------------------------------------------------------------
// Node handlers
// ---------------------------------------------------------------------------
void CppAstVisitor::handleClassSpecifier(TSNode node, const WalkState& state) {
    const TSNode name_node = childByField(node, "name");

    if (ts_node_is_null(name_node)) {
        // Anonymous class/struct — walk body tanpa menaikkan enclosing_class.
        // Method di dalam anonymous class tidak bisa didefinisikan di luar,
        // jadi tidak ada symbol yang terlewat.
        const uint32_t nc = ts_node_named_child_count(node);
        for (uint32_t i = 0; i < nc; ++i) {
            visitNode(ts_node_named_child(node, i), state);
        }
        return;
    }

    const std::string class_name = textOf(name_node);
    if (class_name.empty()) return;

    const std::string qualified = qualify(state, class_name);

    ClassInfo ci;
    ci.name       = qualified;
    ci.start_line = startLine(node);
    ci.end_line   = endLine(node);
    ci.content    = textOf(node);
    ci.signature  = firstLine(ci.content);

    // Base class clause.
    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode child = ts_node_named_child(node, i);
        if (std::strcmp(ts_node_type(child), "base_class_clause") == 0) {
            auto bases = extractBaseClassNames(child);
            for (auto& b : bases) ci.bases.push_back(std::move(b));
        }
    }

    onClass(ci);

    // Walk body dengan enclosing_class diperbarui (nested class → "Outer::Inner").
    WalkState inner = state;
    if (inner.enclosing_class.empty()) {
        inner.enclosing_class = class_name;
    } else {
        inner.enclosing_class += "::" + class_name;
    }
    inner.enclosing_function.clear();   // method di-reset caller context.

    for (uint32_t i = 0; i < nc; ++i) {
        TSNode child = ts_node_named_child(node, i);
        if (ts_node_eq(child, name_node)) continue;
        visitNode(child, inner);
    }
}

void CppAstVisitor::handleStructSpecifier(TSNode node, const WalkState& state) {
    // Struct dan class identik dari sisi symbol extraction (kind="class").
    handleClassSpecifier(node, state);
}

void CppAstVisitor::handleFunctionDefinition(TSNode node,
                                              const WalkState& state) {
    const TSNode fd = childByField(node, "declarator");
    if (ts_node_is_null(fd)) return;
    if (std::strcmp(ts_node_type(fd), "function_declarator") != 0) return;

    const TSNode inner_decl = childByField(fd, "declarator");
    if (ts_node_is_null(inner_decl)) return;

    const std::string symbol_name = declaratorName(inner_decl, state);
    if (symbol_name.empty()) return;

    // FIX 10a (v1.1): parent_class diturunkan lewat inferParentClass()
    // supaya out-of-line method definition (`double Shape::area() {}` di
    // impl file, di luar class body) tetap di-kategorikan sebagai method.
    FunctionInfo fi;
    fi.name         = symbol_name;
    fi.parent_class = inferParentClass(inner_decl, state, symbol_name);
    fi.start_line   = startLine(node);
    fi.end_line     = endLine(node);
    fi.content      = textOf(node);
    fi.signature    = firstLine(fi.content);

    onFunction(fi);

    // Walk body dengan enclosing_function diperbarui, supaya call di
    // dalam body teratribusi ke caller yang tepat.
    WalkState inner = state;
    inner.enclosing_function = symbol_name;

    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        visitNode(ts_node_named_child(node, i), inner);
    }
}

void CppAstVisitor::handleDeclaration(TSNode node, const WalkState& state) {
    // Skip kalau di dalam function body (local declaration — bukan
    // symbol project).
    if (!state.enclosing_function.empty()) return;

    const TSNode decl = childByField(node, "declarator");
    if (ts_node_is_null(decl)) return;
    if (std::strcmp(ts_node_type(decl), "function_declarator") != 0) return;

    const TSNode inner_decl = childByField(decl, "declarator");
    if (ts_node_is_null(inner_decl)) return;

    const std::string symbol_name = declaratorName(inner_decl, state);
    if (symbol_name.empty()) return;

    // FIX 10a (v1.1): sama seperti handleFunctionDefinition — gunakan
    // inferParentClass() supaya method declaration di dalam class body
    // (yang selalu enclosing_class non-empty) dan method declaration
    // out-of-line (`void Shape::area();` di impl) keduanya kind=method.
    FunctionInfo fi;
    fi.name         = symbol_name;
    fi.parent_class = inferParentClass(inner_decl, state, symbol_name);
    fi.start_line   = startLine(node);
    fi.end_line     = endLine(node);
    fi.content      = textOf(node);
    fi.signature    = firstLine(fi.content);

    onFunction(fi);
}

void CppAstVisitor::handleNamespaceDefinition(TSNode node,
                                                const WalkState& state) {
    const TSNode name_node = childByField(node, "name");

    WalkState inner = state;
    if (!ts_node_is_null(name_node)) {
        const std::string ns_name = textOf(name_node);
        if (!ns_name.empty()) {
            if (inner.namespace_scope.empty()) {
                inner.namespace_scope = ns_name;
            } else {
                inner.namespace_scope += "::" + ns_name;
            }
        }
    }
    // Anonymous namespace: namespace_scope tidak berubah, symbol di dalamnya
    // di-qualify seperti di scope induk. Bukan sempurna secara semantik,
    // tapi cukup untuk ContextSelector.

    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        visitNode(ts_node_named_child(node, i), inner);
    }
}

void CppAstVisitor::handlePreprocInclude(TSNode node) {
    const TSNode path = childByField(node, "path");
    if (ts_node_is_null(path)) return;

    const std::string raw     = textOf(path);
    const std::string cleaned = stripIncludeDelims(raw);
    if (cleaned.empty()) return;

    ImportInfo info;
    info.module = cleaned;
    info.line   = startLine(node);
    onImport(info);
}

void CppAstVisitor::handleCallExpression(TSNode node,
                                          const WalkState& state) {
    if (state.enclosing_function.empty()) return;

    const TSNode fn = childByField(node, "function");
    if (ts_node_is_null(fn)) return;

    const std::string callee = extractCallName(fn);
    if (callee.empty()) return;

    CallInfo info;
    info.caller_symbol = state.enclosing_function;
    info.callee_name   = callee;
    info.line          = startLine(node);
    onCall(info);
}

} // namespace frans::code_intelligence
