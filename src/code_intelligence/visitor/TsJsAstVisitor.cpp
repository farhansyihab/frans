// ~/AgenAI/frans/src/code_intelligence/visitor/TsJsAstVisitor.cpp
#include "code_intelligence/visitor/TsJsAstVisitor.h"

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

bool nodeTypeIs(TSNode node, const char* type) {
    return std::strcmp(ts_node_type(node), type) == 0;
}

// Cek apakah node adalah arrow_function atau function_expression.
// Beberapa versi grammar tree-sitter-javascript menggunakan nama
// "function" untuk function expression; kita terima ketiganya.
bool isFunctionValue(TSNode node) {
    const char* t = ts_node_type(node);
    return std::strcmp(t, "arrow_function") == 0
        || std::strcmp(t, "function_expression") == 0
        || std::strcmp(t, "function") == 0;
}

// Cek apakah node adalah expression yang "bernilai" sebagai base class.
// Dipakai di handleClassDeclaration untuk fallback ketika grammar tidak
// mengekspos field name yang kita harapkan.
bool isClassBaseExpression(TSNode node) {
    const char* t = ts_node_type(node);
    return std::strcmp(t, "identifier")        == 0
        || std::strcmp(t, "type_identifier")   == 0
        || std::strcmp(t, "member_expression") == 0
        || std::strcmp(t, "generic_type")      == 0
        || std::strcmp(t, "nested_type_identifier") == 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TsJsAstVisitor::TsJsAstVisitor(std::string file_path,
                                const std::string& source)
    : file_path_(std::move(file_path)), source_(source)
{
    result_.file_path = file_path_;
}

ParsedFile TsJsAstVisitor::takeResult() {
    return std::move(result_);
}

std::string TsJsAstVisitor::textOf(TSNode node) const {
    return toStr(node, source_);
}

// ---------------------------------------------------------------------------
// IAstVisitor accumulation
// ---------------------------------------------------------------------------
void TsJsAstVisitor::onFunction(const FunctionInfo& fi) {
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

void TsJsAstVisitor::onClass(const ClassInfo& ci) {
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

void TsJsAstVisitor::onCall(const CallInfo& info) {
    if (info.caller_symbol.empty() || info.callee_name.empty()) return;
    SymbolRelation r;
    r.from_symbol = info.caller_symbol;
    r.to_symbol   = info.callee_name;
    r.kind        = "calls";
    r.file_path   = file_path_;
    result_.relations.push_back(std::move(r));
}

void TsJsAstVisitor::onImport(const ImportInfo& info) {
    if (!info.module.empty()) {
        result_.imports.push_back(info.module);
    }
    for (const auto& n : info.names) {
        result_.imports.push_back(n);
    }
}

// ---------------------------------------------------------------------------
// Traversal
// ---------------------------------------------------------------------------
void TsJsAstVisitor::walk(TSNode root) {
    WalkState s;
    visitNode(root, s);
}

void TsJsAstVisitor::visitNode(TSNode node, const WalkState& state) {
    if (ts_node_is_null(node)) return;

    const char* type = ts_node_type(node);

    if (std::strcmp(type, "function_declaration") == 0) {
        handleFunctionDeclaration(node, state);
        return;
    }
    if (std::strcmp(type, "class_declaration") == 0) {
        handleClassDeclaration(node, state);
        return;
    }
    if (std::strcmp(type, "lexical_declaration") == 0
     || std::strcmp(type, "variable_declaration") == 0) {
        handleVariableDeclaration(node, state);
        return;
    }
    if (std::strcmp(type, "method_definition") == 0) {
        handleMethodDefinition(node, state);
        return;
    }
    if (std::strcmp(type, "import_statement") == 0) {
        handleImportStatement(node);
        return;
    }
    if (std::strcmp(type, "call_expression") == 0) {
        handleCallExpression(node, state);
        // Fall through — nested calls di dalam arguments perlu di-walk.
    }
    if (std::strcmp(type, "new_expression") == 0) {
        handleNewExpression(node, state);
        // Fall through — nested calls di arguments perlu di-walk.
    }
    // export_statement sengaja TIDAK di-handle khusus: kita biarkan
    // rekursi generik menembus ke declaration (function/class) yang
    // dibungkusnya, sehingga `export function foo()` tetap menghasilkan
    // snippet `foo`.

    // Recurse ke semua named children dengan state yang sama.
    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        visitNode(ts_node_named_child(node, i), state);
    }
}

// ---------------------------------------------------------------------------
// Node handlers
// ---------------------------------------------------------------------------
void TsJsAstVisitor::handleFunctionDeclaration(TSNode node,
                                                const WalkState& state)
{
    const TSNode name_node = childByField(node, "name");
    if (ts_node_is_null(name_node)) return;

    const std::string base = textOf(name_node);

    FunctionInfo fi;
    if (!state.enclosing_function.empty()) {
        fi.name = state.enclosing_function + "." + base;
    } else if (!state.enclosing_class.empty()) {
        fi.name = state.enclosing_class + "." + base;
    } else {
        fi.name = base;
    }
    fi.parent_class = state.enclosing_class;
    fi.start_line   = startLine(node);
    fi.end_line     = endLine(node);
    fi.content      = textOf(node);
    fi.signature    = firstLine(fi.content);

    onFunction(fi);

    // Walk body dengan enclosing_function diperbarui (untuk nested
    // function naming dan call attribution).
    WalkState inner = state;
    inner.enclosing_function = fi.name;

    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode child = ts_node_named_child(node, i);
        if (ts_node_eq(child, name_node)) continue;
        visitNode(child, inner);
    }
}

void TsJsAstVisitor::handleVariableDeclaration(TSNode node,
                                                const WalkState& state)
{
    // Iterate setiap `variable_declarator`. Kalau value-nya adalah
    // arrow_function / function_expression, emit snippet dengan nama =
    // nama variable. Selain itu, walk value-nya untuk menangkap nested
    // call di initializer biasa.
    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode decl = ts_node_named_child(node, i);
        if (!nodeTypeIs(decl, "variable_declarator")) {
            // Grammar varia — walk biasa.
            visitNode(decl, state);
            continue;
        }

        const TSNode name_node  = childByField(decl, "name");
        const TSNode value_node = childByField(decl, "value");

        if (ts_node_is_null(name_node) || ts_node_is_null(value_node)) {
            // Destructuring atau declaration tanpa initializer — walk.
            visitNode(decl, state);
            continue;
        }

        if (!isFunctionValue(value_node)) {
            // Non-function initializer — walk untuk nested calls.
            visitNode(value_node, state);
            continue;
        }

        const std::string base = textOf(name_node);

        FunctionInfo fi;
        if (!state.enclosing_function.empty()) {
            fi.name = state.enclosing_function + "." + base;
        } else if (!state.enclosing_class.empty()) {
            fi.name = state.enclosing_class + "." + base;
        } else {
            fi.name = base;
        }
        fi.parent_class = state.enclosing_class;
        // Line range menunjuk ke SELURUH declarator (termasuk `const`)
        // supaya model bisa melihat binding-nya. Content = textOf(decl)
        // → "mul = (a, b) => a * b" (tanpa `const`).
        fi.start_line = startLine(decl);
        fi.end_line   = endLine(decl);
        fi.content    = textOf(decl);
        fi.signature  = firstLine(fi.content);

        onFunction(fi);

        // Walk body arrow function untuk nested calls.
        WalkState inner = state;
        inner.enclosing_function = fi.name;
        visitNode(value_node, inner);
    }
}

void TsJsAstVisitor::handleClassDeclaration(TSNode node,
                                             const WalkState& state)
{
    const TSNode name_node = childByField(node, "name");
    if (ts_node_is_null(name_node)) return;

    const std::string base = textOf(name_node);

    ClassInfo ci;
    if (!state.enclosing_class.empty()) {
        ci.name = state.enclosing_class + "." + base;
    } else if (!state.enclosing_function.empty()) {
        ci.name = state.enclosing_function + "." + base;
    } else {
        ci.name = base;
    }
    ci.start_line = startLine(node);
    ci.end_line   = endLine(node);
    ci.content    = textOf(node);
    ci.signature  = firstLine(ci.content);

    // -------------------------------------------------------------------
    // Class heritage (`extends X`) — grammar-dependent.
    //
    // tree-sitter-javascript:
    //     (class_heritage (identifier))
    //     → expression ada langsung sebagai named child.
    //
    // tree-sitter-typescript:
    //     (class_heritage
    //       (extends_clause value: (identifier))
    //       (implements_clause ...))   ← ini BUKAN inheritance, skip
    //     → expression dibungkus di dalam extends_clause.
    //
    // Handler di bawah menangani kedua bentuk: cari extends_clause dulu
    // (TS style), kalau tidak ketemu, cek child langsung yang berupa
    // identifier/member_expression (JS style).
    // -------------------------------------------------------------------
    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode heritage = ts_node_named_child(node, i);
        if (!nodeTypeIs(heritage, "class_heritage")) continue;

        const uint32_t hc = ts_node_named_child_count(heritage);
        for (uint32_t j = 0; j < hc; ++j) {
            TSNode inner = ts_node_named_child(heritage, j);
            const char* t = ts_node_type(inner);

            // implements_clause bukan inheritance — skip.
            if (std::strcmp(t, "implements_clause") == 0) continue;

            // TS style: extends_clause wrapper.
            if (std::strcmp(t, "extends_clause") == 0) {
                TSNode v = childByField(inner, "value");
                if (ts_node_is_null(v)
                    && ts_node_named_child_count(inner) > 0) {
                    v = ts_node_named_child(inner, 0);
                }
                if (!ts_node_is_null(v)) {
                    ci.bases.push_back(textOf(v));
                }
                continue;
            }

            // JS style: expression langsung sebagai child.
            if (isClassBaseExpression(inner)) {
                ci.bases.push_back(textOf(inner));
            }
        }
    }

    onClass(ci);

    // Recurse body dengan enclosing_class diperbarui. enclosing_function
    // di-reset karena method baru (tidak inherit enclosing_function dari
    // luar class).
    WalkState inner = state;
    inner.enclosing_class    = ci.name;
    inner.enclosing_function = "";

    for (uint32_t i = 0; i < nc; ++i) {
        TSNode child = ts_node_named_child(node, i);
        if (ts_node_eq(child, name_node)) continue;
        visitNode(child, inner);
    }
}

void TsJsAstVisitor::handleMethodDefinition(TSNode node,
                                             const WalkState& state)
{
    // `name` field menunjuk ke property_identifier, yang untuk
    // getter/setter pun tetap nama method tanpa prefix `get`/`set`
    // (tree-sitter menyimpan keyword `get`/`set` sebagai anonymous
    // child, bukan bagian dari `name`).
    const TSNode name_node = childByField(node, "name");
    if (ts_node_is_null(name_node)) return;

    const std::string base = textOf(name_node);

    FunctionInfo fi;
    if (!state.enclosing_class.empty()) {
        fi.name = state.enclosing_class + "." + base;
    } else {
        fi.name = base;
    }
    fi.parent_class = state.enclosing_class;
    fi.start_line   = startLine(node);
    fi.end_line     = endLine(node);
    fi.content      = textOf(node);
    fi.signature    = firstLine(fi.content);

    onFunction(fi);

    // Walk body dengan enclosing_function diperbarui.
    WalkState inner = state;
    inner.enclosing_function = fi.name;

    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode child = ts_node_named_child(node, i);
        if (ts_node_eq(child, name_node)) continue;
        visitNode(child, inner);
    }
}

void TsJsAstVisitor::handleImportStatement(TSNode node) {
    // import_statement punya field `source` yang menunjuk ke string
    // literal. Kita tidak peduli bentuk clause-nya (`import X from`,
    // `import { X } from`, `import * as X from`, `import "./side-effect"`).
    const TSNode source = childByField(node, "source");
    if (ts_node_is_null(source)) return;

    ImportInfo info;
    info.module = extractModuleSpecifier(source);
    info.line   = startLine(node);
    if (!info.module.empty()) {
        onImport(info);
    }
}

void TsJsAstVisitor::handleCallExpression(TSNode node,
                                           const WalkState& state)
{
    const std::string callee = extractCallName(node);
    if (callee.empty()) return;

    // ---- Special case: require("...") → imports ----
    // CommonJS style. Di-anggap sebagai import karena semantiknya sama
    // (mengimpor modul).
    if (callee == "require") {
        const TSNode args = childByField(node, "arguments");
        if (!ts_node_is_null(args) && ts_node_named_child_count(args) > 0) {
            const TSNode first = ts_node_named_child(args, 0);
            if (nodeTypeIs(first, "string")
             || nodeTypeIs(first, "template_string")) {
                ImportInfo info;
                info.module = extractModuleSpecifier(first);
                info.line   = startLine(node);
                if (!info.module.empty()) onImport(info);
            }
        }
        return;
    }

    // Top-level calls (di luar function/method) tidak punya caller
    // symbol untuk diatribusi. Skip — relations butuh from_symbol.
    if (state.enclosing_function.empty()) return;

    CallInfo info;
    info.caller_symbol = state.enclosing_function;
    info.callee_name   = callee;
    info.line          = startLine(node);
    onCall(info);
}

void TsJsAstVisitor::handleNewExpression(TSNode node,
                                          const WalkState& state)
{
    // `new Circle()` → relation "calls" dari enclosing function ke
    // "Circle" (constructor). Blueprint assertion #5 mensyaratkan ini.
    if (state.enclosing_function.empty()) return;

    const TSNode ctor = childByField(node, "constructor");
    if (ts_node_is_null(ctor)) return;

    std::string name;
    const char* t = ts_node_type(ctor);
    if (std::strcmp(t, "identifier") == 0
     || std::strcmp(t, "type_identifier") == 0) {
        name = textOf(ctor);
    } else if (std::strcmp(t, "member_expression") == 0) {
        const TSNode prop = childByField(ctor, "property");
        if (!ts_node_is_null(prop)) name = textOf(prop);
    }
    if (name.empty()) return;

    CallInfo info;
    info.caller_symbol = state.enclosing_function;
    info.callee_name   = name;
    info.line          = startLine(node);
    onCall(info);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
std::string TsJsAstVisitor::extractModuleSpecifier(
    TSNode string_node) const
{
    const std::string raw = textOf(string_node);
    if (raw.size() >= 2) {
        const char first = raw.front();
        const char last  = raw.back();
        if ((first == '"'  && last == '"')
         || (first == '\'' && last == '\'')) {
            return raw.substr(1, raw.size() - 2);
        }
    }
    return raw;
}

std::string TsJsAstVisitor::extractCallName(TSNode call_expr) const {
    const TSNode fn = childByField(call_expr, "function");
    if (ts_node_is_null(fn)) return {};

    const char* t = ts_node_type(fn);
    if (std::strcmp(t, "identifier") == 0) {
        return textOf(fn);
    }
    if (std::strcmp(t, "member_expression") == 0) {
        const TSNode prop = childByField(fn, "property");
        if (!ts_node_is_null(prop)) return textOf(prop);
    }
    // Callable dalam bentuk lain (subscript, generic call, dll) —
    // skip untuk sekarang (blueprint §4.3 out-of-scope).
    return {};
}

} // namespace frans::code_intelligence
