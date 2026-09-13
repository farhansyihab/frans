// ~/AgenAI/frans/src/code_intelligence/visitor/IAstVisitor.h
#pragma once

#include <string>
#include <vector>

namespace frans::code_intelligence {

// Blueprint §4 — Visitor. Dipanggil oleh walker bahasa-spesifik saat
// menemukan node menarik di AST. Event payload sengaja semantic (bukan
// TSNode) supaya implementasi visitor tidak tergantung tree-sitter.
//
// Catatan: traversal tree-sitter memakai urutan DFS natural — visitor
// TIDAK boleh berasumsi urutan antar-event. Semua hasil akhir di-sort
// oleh caller sebelum dipakai.

struct FunctionInfo {
    std::string name;
    std::string parent_class;    // "" kalau top-level
    int         start_line = 0;
    int         end_line   = 0;
    std::string signature;
    std::string content;
};

struct ClassInfo {
    std::string name;
    std::vector<std::string> bases;
    int         start_line = 0;
    int         end_line   = 0;
    std::string signature;
    std::string content;
};

struct CallInfo {
    std::string caller_symbol;   // "" kalau top-level
    std::string callee_name;     // nama yang dipanggil (belum di-resolve)
    int         line = 0;
};

struct ImportInfo {
    std::string module;                // "os", "pkg.sub"
    std::vector<std::string> names;    // `from x import a, b` → {a, b}
    int         line = 0;
};

class IAstVisitor {
public:
    virtual ~IAstVisitor() = default;

    virtual void onFunction(const FunctionInfo&) = 0;
    virtual void onClass(const ClassInfo&) = 0;
    virtual void onCall(const CallInfo&) = 0;
    virtual void onImport(const ImportInfo&) = 0;
};

} // namespace frans::code_intelligence
