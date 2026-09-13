// ~/AgenAI/frans/src/code_intelligence/visitor/PythonAstVisitor.cpp
#include "code_intelligence/visitor/PythonAstVisitor.h"

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
    return ts_node_child_by_field_name(node, field,
                                       static_cast<uint32_t>(std::strlen(field)));
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
PythonAstVisitor::PythonAstVisitor(std::string file_path,
                                   const std::string& source)
    : file_path_(std::move(file_path)), source_(source) {
    source_ref_ = &source_;
    result_.file_path = file_path_;
}

ParsedFile PythonAstVisitor::takeResult() {
    return std::move(result_);
}

std::string PythonAstVisitor::textOf(TSNode node) const {
    return toStr(node, *source_ref_);
}

// ---------------------------------------------------------------------------
// IAstVisitor accumulation
// ---------------------------------------------------------------------------
void PythonAstVisitor::onFunction(const FunctionInfo& fi) {
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

void PythonAstVisitor::onClass(const ClassInfo& ci) {
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

void PythonAstVisitor::onCall(const CallInfo& info) {
    if (info.caller_symbol.empty() || info.callee_name.empty()) return;
    SymbolRelation r;
    r.from_symbol = info.caller_symbol;
    r.to_symbol   = info.callee_name;
    r.kind        = "calls";
    r.file_path   = file_path_;
    result_.relations.push_back(std::move(r));
}

void PythonAstVisitor::onImport(const ImportInfo& info) {
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
void PythonAstVisitor::walk(TSNode root) {
    WalkState s;
    visitNode(root, s);
}

void PythonAstVisitor::visitNode(TSNode node, const WalkState& state) {
    if (ts_node_is_null(node)) return;

    const char* type = ts_node_type(node);

    if (std::strcmp(type, "function_definition") == 0) {
        handleFunction(node, state);
        return;
    }
    if (std::strcmp(type, "class_definition") == 0) {
        handleClass(node, state);
        return;
    }
    if (std::strcmp(type, "call") == 0) {
        handleCall(node, state);
        // Jangan return — lanjutkan walk ke argumen, karena nested call
        // seperti f(g()) tetap perlu tercatat.
    }
    if (std::strcmp(type, "import_statement") == 0) {
        handleImport(node);
    }
    if (std::strcmp(type, "import_from_statement") == 0) {
        handleImportFrom(node);
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
void PythonAstVisitor::handleFunction(TSNode node, const WalkState& state) {
    const TSNode name_node = childByField(node, "name");
    if (ts_node_is_null(name_node)) return;

    FunctionInfo fi;
    fi.name         = textOf(name_node);
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
        // Skip name & parameters (tidak perlu di-traverse).
        if (ts_node_eq(child, name_node)) continue;
        visitNode(child, inner);
    }
}

void PythonAstVisitor::handleClass(TSNode node, const WalkState& state) {
    const TSNode name_node = childByField(node, "name");
    if (ts_node_is_null(name_node)) return;

    ClassInfo ci;
    ci.name       = textOf(name_node);
    ci.start_line = startLine(node);
    ci.end_line   = endLine(node);
    ci.content    = textOf(node);
    ci.signature  = firstLine(ci.content);

    // Superclass: field "superclasses" adalah argument_list (opsional).
    const TSNode supers = childByField(node, "superclasses");
    if (!ts_node_is_null(supers)) {
        const uint32_t nc = ts_node_named_child_count(supers);
        for (uint32_t i = 0; i < nc; ++i) {
            TSNode base = ts_node_named_child(supers, i);
            std::string b = textOf(base);
            // Buang whitespace / keyword argument kalau ada.
            if (!b.empty()) ci.bases.push_back(std::move(b));
        }
    }

    onClass(ci);

    // Walk body dengan enclosing_class diperbarui. Method juga
    // mendapat enclosing_function="" supaya nama method tidak
    // tumpang tindih dengan nama class.
    WalkState inner = state;
    inner.enclosing_class    = ci.name;
    inner.enclosing_function = "";

    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode child = ts_node_named_child(node, i);
        if (ts_node_eq(child, name_node)) continue;
        if (!ts_node_is_null(supers) && ts_node_eq(child, supers)) continue;
        visitNode(child, inner);
    }
}

std::string PythonAstVisitor::attributeTailName(TSNode attr_node) const {
    // `obj.method` → attribute field = "method".
    const TSNode tail = childByField(attr_node, "attribute");
    if (!ts_node_is_null(tail)) return textOf(tail);
    return {};
}

bool PythonAstVisitor::isSelfAttribute(TSNode attr_node) const {
    const TSNode obj = childByField(attr_node, "object");
    if (ts_node_is_null(obj)) return false;
    return std::strcmp(ts_node_type(obj), "identifier") == 0
        && textOf(obj) == "self";
}

void PythonAstVisitor::handleCall(TSNode node, const WalkState& state) {
    const TSNode fn = childByField(node, "function");
    if (ts_node_is_null(fn)) return;

    std::string callee;
    const char* fn_type = ts_node_type(fn);

    if (std::strcmp(fn_type, "identifier") == 0) {
        callee = textOf(fn);
    } else if (std::strcmp(fn_type, "attribute") == 0) {
        // Hanya self.method() yang di-resolve — obj.method() butuh
        // type info yang tidak tersedia di stage 5.
        if (!isSelfAttribute(fn)) return;
        callee = attributeTailName(fn);
    } else {
        // callable, subscript, dll — skip.
        return;
    }

    if (callee.empty()) return;

    CallInfo info;
    info.caller_symbol = state.enclosing_function;
    info.callee_name   = std::move(callee);
    info.line          = startLine(node);
    onCall(info);
}

void PythonAstVisitor::handleImport(TSNode node) {
    // `import os` | `import os.path` | `import os as o`
    const uint32_t nc = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < nc; ++i) {
        TSNode child = ts_node_named_child(node, i);
        const char* t = ts_node_type(child);
        if (std::strcmp(t, "dotted_name") == 0) {
            ImportInfo info;
            info.module = textOf(child);
            info.line   = startLine(node);
            onImport(info);
        } else if (std::strcmp(t, "aliased_import") == 0) {
            ImportInfo info;
            const TSNode name = childByField(child, "name");
            info.module = ts_node_is_null(name) ? textOf(child) : textOf(name);
            info.line   = startLine(node);
            onImport(info);
        }
    }
}

void PythonAstVisitor::handleImportFrom(TSNode node) {
    // `from X import a, b` | `from . import x` | `from X import a as b`
    ImportInfo info;
    info.line = startLine(node);

    const uint32_t nc = ts_node_named_child_count(node);
    if (nc == 0) return;

    // Named child pertama = module (dotted_name / relative_import).
    TSNode mod = ts_node_named_child(node, 0);
    info.module = textOf(mod);

    // Sisanya = nama-nama yang di-import.
    for (uint32_t i = 1; i < nc; ++i) {
        TSNode child = ts_node_named_child(node, i);
        const char* t = ts_node_type(child);
        if (std::strcmp(t, "aliased_import") == 0) {
            const TSNode name = childByField(child, "name");
            info.names.push_back(ts_node_is_null(name) ? textOf(child)
                                                       : textOf(name));
        } else if (std::strcmp(t, "dotted_name") == 0
                || std::strcmp(t, "identifier") == 0) {
            info.names.push_back(textOf(child));
        }
    }

    onImport(info);
}

} // namespace frans::code_intelligence
