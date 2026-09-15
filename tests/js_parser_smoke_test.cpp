// ~/AgenAI/frans/tests/js_parser_smoke_test.cpp
//
// Blueprint Stage 10b — smoke test untuk JavaScriptParser.
//
// Fixture di tests/fixtures/javascript/ (path di-inject lewat
// FIXTURE_DIR compile definition).

#include "code_intelligence/parsers/CodeSnippet.h"
#include "code_intelligence/parsers/JavaScriptParser.h"
#include "code_intelligence/parsers/ParsedFile.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

int g_checks   = 0;
int g_failures = 0;

void check(bool cond, const std::string& label) {
    ++g_checks;
    if (cond) {
        std::fprintf(stderr, "  PASS: %s\n", label.c_str());
    } else {
        ++g_failures;
        std::fprintf(stderr, "  FAIL: %s\n", label.c_str());
    }
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "  [cannot open: %s]\n", path.c_str());
        return {};
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string fixture(const std::string& rel) {
    return std::string(FIXTURE_DIR) + "/" + rel;
}

const frans::code_intelligence::CodeSnippet* findSymbol(
    const frans::code_intelligence::ParsedFile& pf,
    const std::string& name,
    const std::string& kind = "")
{
    for (const auto& s : pf.snippets) {
        if (s.symbol_name != name) continue;
        if (!kind.empty() && s.kind != kind) continue;
        return &s;
    }
    return nullptr;
}

bool hasImport(const frans::code_intelligence::ParsedFile& pf,
               const std::string& module)
{
    for (const auto& m : pf.imports) {
        if (m == module) return true;
    }
    return false;
}

bool hasRelation(const frans::code_intelligence::ParsedFile& pf,
                 const std::string& from,
                 const std::string& to,
                 const std::string& kind)
{
    for (const auto& r : pf.relations) {
        if (r.from_symbol == from && r.to_symbol == to && r.kind == kind) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------
void testShapes() {
    std::fprintf(stderr, "[case] shapes.js\n");
    const std::string src = readFile(fixture("shapes.js"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::JavaScriptParser p;
    const auto pf = p.parse(fixture("shapes.js"), src);

    check(findSymbol(pf, "Shape", "class")          != nullptr, "Shape class");
    check(findSymbol(pf, "Circle", "class")         != nullptr, "Circle class");
    check(findSymbol(pf, "Shape.area", "method")    != nullptr, "Shape.area method");
    check(findSymbol(pf, "Circle.radius", "method") != nullptr, "Circle.radius method");
    check(findSymbol(pf, "Circle.area", "method")   != nullptr, "Circle.area method");
    check(hasRelation(pf, "Circle", "Shape", "inherits"),
          "relation Circle --inherits--> Shape");
}

void testUtils() {
    std::fprintf(stderr, "[case] utils.js\n");
    const std::string src = readFile(fixture("utils.js"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::JavaScriptParser p;
    const auto pf = p.parse(fixture("utils.js"), src);

    check(findSymbol(pf, "add", "function") != nullptr, "add function");
    check(findSymbol(pf, "mul", "function") != nullptr,
          "mul function (dari arrow assignment)");
}

void testMainRequire() {
    std::fprintf(stderr, "[case] main.js (require style)\n");
    const std::string src = readFile(fixture("main.js"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::JavaScriptParser p;
    const auto pf = p.parse(fixture("main.js"), src);

    check(hasImport(pf, "./shapes"),
          "require('./shapes') → imports berisi './shapes'");
    check(hasRelation(pf, "main", "Circle", "calls"),
          "main --calls--> Circle (constructor)");
    check(hasRelation(pf, "main", "area", "calls"),
          "main --calls--> area");
}

void testEsmMjs() {
    std::fprintf(stderr, "[case] esm.mjs (ESM import)\n");
    const std::string src = readFile(fixture("esm.mjs"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::JavaScriptParser p;
    const auto pf = p.parse(fixture("esm.mjs"), src);

    check(hasImport(pf, "./shapes.mjs"),
          "import './shapes.mjs' → imports berisi './shapes.mjs'");
    check(findSymbol(pf, "run", "function") != nullptr, "run function");
}

void testEmptyInput() {
    std::fprintf(stderr, "[case] empty input\n");
    frans::code_intelligence::JavaScriptParser p;
    const auto pf = p.parse("empty.js", "");
    check(pf.snippets.empty(), "empty source → snippets kosong");
    check(pf.relations.empty(), "empty source → relations kosong");
    check(pf.imports.empty(), "empty source → imports kosong");
}

void testDeterminism() {
    std::fprintf(stderr, "[case] determinism\n");
    const std::string src = readFile(fixture("shapes.js"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::JavaScriptParser p;
    const auto a = p.parse(fixture("shapes.js"), src);
    const auto b = p.parse(fixture("shapes.js"), src);

    bool same = (a.snippets.size() == b.snippets.size());
    if (same) {
        for (std::size_t i = 0; i < a.snippets.size(); ++i) {
            if (a.snippets[i].symbol_name != b.snippets[i].symbol_name
             || a.snippets[i].start_line != b.snippets[i].start_line
             || a.snippets[i].end_line   != b.snippets[i].end_line) {
                same = false;
                break;
            }
        }
    }
    check(same, "dua panggilan parse() menghasilkan output identik");
}

} // namespace

int main() {
    std::fprintf(stderr, "=== frans_js_parser_test ===\n");

    testShapes();
    testUtils();
    testMainRequire();
    testEsmMjs();
    testEmptyInput();
    testDeterminism();

    std::fprintf(stderr,
        "\n=== %d checks, %d failures ===\n", g_checks, g_failures);
    std::fprintf(stderr, "total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
