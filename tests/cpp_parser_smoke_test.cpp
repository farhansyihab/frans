// ~/AgenAI/frans/tests/cpp_parser_smoke_test.cpp
//
// Stage 10a — smoke test untuk CppParser + CppAstVisitor.
//
// Cakupan: 14 assertion dari blueprint §10a.7.
//
// Fixture root: FIXTURE_DIR macro (di-set dari CMakeLists).
//
// Test dijalankan standalone — tidak butuh tree-sitter-python, tidak
// butuh DependencyGraph parser-builder (yang masih Python-only di 10a).

#include "code_intelligence/graph/DependencyGraph.h"
#include "code_intelligence/parsers/CppParser.h"
#include "code_intelligence/parsers/ParsedFile.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using frans::code_intelligence::CppParser;
using frans::code_intelligence::DependencyGraph;
using frans::code_intelligence::ParsedFile;
using frans::code_intelligence::CodeSnippet;
using frans::code_intelligence::SymbolRelation;

// ---------------------------------------------------------------------------
// Tiny assertion framework — cukup untuk smoke test standalone.
// ---------------------------------------------------------------------------
namespace {

int g_failures = 0;
int g_total    = 0;

// `detail` default kosong. Semua pemanggil WAJIB pakai std::string untuk
// argumen ketiga (kalau ada) — helper ini tidak menerima const char*
// supaya tidak ada ambiguitas dan untuk menghindari bug string-concat
// seperti yang sempat ada di versi sebelumnya.
void check(bool cond, const char* label, const std::string& detail = {}) {
    ++g_total;
    if (cond) {
        std::printf("  [PASS] %s\n", label);
    } else {
        ++g_failures;
        std::printf("  [FAIL] %s\n", label);
        if (!detail.empty()) {
            std::printf("         detail: %s\n", detail.c_str());
        }
    }
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string fixturePath(const std::string& name) {
    return std::string(FIXTURE_DIR) + "/" + name;
}

// Return true kalau ada snippet dengan symbol_name + (opsional) kind sesuai.
bool hasSnippet(const ParsedFile& pf, const std::string& name,
                const std::string& kind = "") {
    for (const auto& s : pf.snippets) {
        if (s.symbol_name == name && (kind.empty() || s.kind == kind)) {
            return true;
        }
    }
    return false;
}

const CodeSnippet* findSnippet(const ParsedFile& pf,
                               const std::string& name) {
    for (const auto& s : pf.snippets) {
        if (s.symbol_name == name) return &s;
    }
    return nullptr;
}

bool hasImport(const ParsedFile& pf, const std::string& module) {
    for (const auto& m : pf.imports) {
        if (m == module) return true;
    }
    return false;
}

bool hasRelation(const ParsedFile& pf,
                 const std::string& from,
                 const std::string& to,
                 const std::string& kind) {
    for (const auto& r : pf.relations) {
        if (r.from_symbol == from && r.to_symbol == to && r.kind == kind) {
            return true;
        }
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    std::printf("=== Stage 10a — CppParser smoke test ===\n");
    std::printf("FIXTURE_DIR = %s\n\n", FIXTURE_DIR);

    CppParser parser;

    // -------------------------------------------------------------------
    // Parse tiap fixture sekali — dipakai untuk assertion #1, #2, #5-#8.
    // -------------------------------------------------------------------
    const ParsedFile shape_hpp   = parser.parse(fixturePath("shape.hpp"),
                                                 readFile(fixturePath("shape.hpp")));
    const ParsedFile shape_cpp   = parser.parse(fixturePath("shape.cpp"),
                                                 readFile(fixturePath("shape.cpp")));
    const ParsedFile circle_hpp  = parser.parse(fixturePath("circle.hpp"),
                                                 readFile(fixturePath("circle.hpp")));
    const ParsedFile circle_cpp  = parser.parse(fixturePath("circle.cpp"),
                                                 readFile(fixturePath("circle.cpp")));
    const ParsedFile main_cpp    = parser.parse(fixturePath("main.cpp"),
                                                 readFile(fixturePath("main.cpp")));
    const ParsedFile ns_hpp      = parser.parse(fixturePath("ns.hpp"),
                                                 readFile(fixturePath("ns.hpp")));
    const ParsedFile call_cpp    = parser.parse(fixturePath("call.cpp"),
                                                 readFile(fixturePath("call.cpp")));

    // -------------------------------------------------------------------
    // Assertion #1 — parse shape.hpp saja.
    // -------------------------------------------------------------------
    std::printf("[1] parse shape.hpp saja\n");
    check(hasSnippet(shape_hpp, "Shape", "class"),
          "snippet 'Shape' (kind=class) ada");
    check(hasSnippet(shape_hpp, "Shape::area", "method"),
          "snippet 'Shape::area' (kind=method) ada");
    check(hasSnippet(shape_hpp, "Shape::perimeter", "method"),
          "snippet 'Shape::perimeter' (kind=method) ada");
    {
        const auto* s = findSnippet(shape_hpp, "Shape::area");
        check(s != nullptr && s->signature.find(';') != std::string::npos,
              "signature 'Shape::area' diakhiri ';' (declaration-only)",
              s ? ("signature=" + s->signature) : std::string("snippet missing"));
    }

    // -------------------------------------------------------------------
    // Assertion #2 — parse shape.cpp saja → Shape::area dengan body.
    // -------------------------------------------------------------------
    std::printf("\n[2] parse shape.cpp saja\n");
    {
        const auto* s = findSnippet(shape_cpp, "Shape::area");
        check(s != nullptr && s->kind == "method",
              "snippet 'Shape::area' (kind=method) ada");
        check(s != nullptr
                && s->content.find('{') != std::string::npos
                && s->content.find('}') != std::string::npos,
              "content 'Shape::area' mengandung body '{...}'",
              s ? ("content len=" + std::to_string(s->content.size()))
                : std::string("missing"));
    }

    // -------------------------------------------------------------------
    // Assertion #3 — dual-file merge via DependencyGraph.
    // Build graph manual (builder masih Python-only di 10a).
    // -------------------------------------------------------------------
    std::printf("\n[3] dual-file merge — Shape::area & Circle::area\n");
    {
        DependencyGraph graph;
        std::error_code ec;
        for (auto it = fs::directory_iterator(FIXTURE_DIR, ec);
             !ec && it != fs::directory_iterator(); it.increment(ec))
        {
            const auto& p = it->path();
            if (!p.has_extension()) continue;
            const auto ext = p.extension().string();
            if (ext != ".hpp" && ext != ".h" && ext != ".hxx"
             && ext != ".cpp" && ext != ".cc" && ext != ".cxx") continue;
            const std::string src = readFile(p.string());
            graph.addFile(parser.parse(p.string(), src));
        }

        const auto shape_area = graph.findBySymbol("Shape::area");
        check(shape_area.size() >= 2,
              "graph.findBySymbol(\"Shape::area\") ≥ 2 snippet",
              "count=" + std::to_string(shape_area.size()));

        const auto circle_area = graph.findBySymbol("Circle::area");
        check(circle_area.size() >= 2,
              "graph.findBySymbol(\"Circle::area\") ≥ 2 snippet",
              "count=" + std::to_string(circle_area.size()));

        // Verifikasi bahwa minimal satu dari header (.hpp) dan satu dari .cpp.
        bool from_hpp = false;
        bool from_cpp = false;
        for (const auto& s : circle_area) {
            if (s.file_path.find(".hpp") != std::string::npos) from_hpp = true;
            if (s.file_path.find(".cpp") != std::string::npos) from_cpp = true;
        }
        check(from_hpp && from_cpp,
              "Circle::area datang dari .hpp DAN .cpp");
    }

    // -------------------------------------------------------------------
    // Assertion #4 — inheritance Circle : public Shape.
    // -------------------------------------------------------------------
    std::printf("\n[4] inheritance Circle : public Shape\n");
    check(hasRelation(circle_hpp, "Circle", "Shape", "inherits"),
          "relation Circle --inherits--> Shape ada");

    // -------------------------------------------------------------------
    // Assertion #5 — imports circle.cpp → "circle.hpp".
    // -------------------------------------------------------------------
    std::printf("\n[5] imports circle.cpp\n");
    check(hasImport(circle_cpp, "circle.hpp"),
          "import 'circle.hpp' ada (tanpa quote/bracket)",
          "imports count=" + std::to_string(circle_cpp.imports.size()));

    // -------------------------------------------------------------------
    // Assertion #6 — calls di call.cpp.
    // -------------------------------------------------------------------
    std::printf("\n[6] calls di call.cpp\n");
    check(hasRelation(call_cpp, "caller", "helper", "calls"),
          "relation caller --calls--> helper ada");

    // -------------------------------------------------------------------
    // Assertion #7 — namespace qualification (class + method).
    // -------------------------------------------------------------------
    std::printf("\n[7] namespace qualification\n");
    check(hasSnippet(ns_hpp, "foo::bar::Baz", "class"),
          "snippet 'foo::bar::Baz' (kind=class) ada");
    check(hasSnippet(ns_hpp, "foo::bar::Baz::qux", "method"),
          "snippet 'foo::bar::Baz::qux' (kind=method) ada");

    // -------------------------------------------------------------------
    // Assertion #8 — fully-qualified top-level free function di namespace.
    // -------------------------------------------------------------------
    std::printf("\n[8] top-level function di namespace\n");
    check(hasSnippet(ns_hpp, "foo::bar::free_function", "function"),
          "snippet 'foo::bar::free_function' (kind=function) ada");

    // -------------------------------------------------------------------
    // Assertion #9 — determinism.
    // -------------------------------------------------------------------
    std::printf("\n[9] determinism\n");
    {
        const std::string src = readFile(fixturePath("circle.cpp"));
        const ParsedFile a = parser.parse("circle.cpp", src);
        const ParsedFile b = parser.parse("circle.cpp", src);

        const bool same_snippet_count = a.snippets.size() == b.snippets.size();
        bool same_order = same_snippet_count;
        if (same_order) {
            for (std::size_t i = 0; i < a.snippets.size(); ++i) {
                if (a.snippets[i].symbol_name != b.snippets[i].symbol_name
                 || a.snippets[i].start_line  != b.snippets[i].start_line) {
                    same_order = false;
                    break;
                }
            }
        }
        check(same_order, "dua parse() menghasilkan urutan snippet identik");
    }

    // -------------------------------------------------------------------
    // Assertion #10 — empty file.
    // -------------------------------------------------------------------
    std::printf("\n[10] empty file\n");
    {
        const ParsedFile empty = parser.parse("empty.cpp", "");
        check(empty.snippets.empty() && empty.imports.empty(),
              "parse() empty → ParsedFile kosong (no crash)");
    }

    // -------------------------------------------------------------------
    // Assertion #11 — syntax error (tree-sitter toleran).
    //
    // Catatan: label `check()` hanya menerima const char*. Detail dinamis
    // (snippet count) DI-PASS lewat argumen ketiga (std::string), bukan
    // dikonkat ke label.
    // -------------------------------------------------------------------
    std::printf("\n[11] syntax error toleran\n");
    {
        const ParsedFile broken = parser.parse("broken.cpp", "class {\n");
        check(true,
              "parse() broken → tidak crash",
              "snippet count=" + std::to_string(broken.snippets.size()));

        // Syntax error di tengah file: symbol valid sebelum error harus
        // tetap di-extract (tree-sitter error-recovery).
        const std::string src = "int good_fn() { return 1; }\nclass {";
        const ParsedFile partial = parser.parse("partial.cpp", src);
        check(hasSnippet(partial, "good_fn", "function"),
              "symbol valid sebelum syntax error tetap di-extract");
    }

    // -------------------------------------------------------------------
    // Assertion #12 — method call via this-> (di dalam method lain).
    // -------------------------------------------------------------------
    std::printf("\n[12] method call via this->\n");
    {
        const std::string src =
            "class Foo {\n"
            "public:\n"
            "    double area() const { return 0.0; }\n"
            "    double describe() const {\n"
            "        return this->area();\n"
            "    }\n"
            "};\n";
        const ParsedFile pf = parser.parse("this_call.cpp", src);
        check(hasRelation(pf, "Foo::describe", "area", "calls"),
              "relation Foo::describe --calls--> area (leaf name)",
              "relations=" + std::to_string(pf.relations.size()));
    }

    // -------------------------------------------------------------------
    // Assertion #13 — method call via obj.method().
    // -------------------------------------------------------------------
    std::printf("\n[13] method call via obj.method()\n");
    {
        check(hasRelation(main_cpp, "main", "area", "calls"),
              "relation main --calls--> area");
        check(hasRelation(main_cpp, "main", "radius", "calls"),
              "relation main --calls--> radius");
    }

    // -------------------------------------------------------------------
    // Assertion #14 — method call via ptr->method().
    // -------------------------------------------------------------------
    std::printf("\n[14] method call via ptr->method()\n");
    {
        const std::string src =
            "struct Shape { double area() const; };\n"
            "double use(Shape* p) {\n"
            "    return p->area();\n"
            "}\n";
        const ParsedFile pf = parser.parse("arrow.cpp", src);
        check(hasRelation(pf, "use", "area", "calls"),
              "relation use --calls--> area (dari ptr->area())");
    }

    // -------------------------------------------------------------------
    // Summary
    // -------------------------------------------------------------------
    std::printf("\n=== Summary ===\n");
    std::printf("checks: %d, failures: %d\n", g_total, g_failures);
    std::printf(g_failures == 0 ? "ALL PASS\n" : "FAILURES DETECTED\n");

    return g_failures == 0 ? 0 : 1;
}
