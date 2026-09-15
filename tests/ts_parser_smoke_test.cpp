// ~/AgenAI/frans/tests/ts_parser_smoke_test.cpp
//
// Blueprint Stage 10b — smoke test untuk TypeScriptParser.
//
// Fixture di tests/fixtures/typescript/ (path di-inject lewat
// FIXTURE_DIR compile definition).

#include "code_intelligence/parsers/CodeSnippet.h"
#include "code_intelligence/parsers/ParsedFile.h"
#include "code_intelligence/parsers/TypeScriptParser.h"

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
    std::fprintf(stderr, "[case] shapes.ts\n");
    const std::string src = readFile(fixture("shapes.ts"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::TypeScriptParser p;
    const auto pf = p.parse(fixture("shapes.ts"), src);

    check(findSymbol(pf, "Shape", "class")        != nullptr, "Shape class");
    check(findSymbol(pf, "Circle", "class")       != nullptr, "Circle class");
    check(findSymbol(pf, "Shape.area", "method")  != nullptr, "Shape.area method");
    check(findSymbol(pf, "Circle.radius", "method") != nullptr, "Circle.radius method");
    check(findSymbol(pf, "Circle.area", "method") != nullptr, "Circle.area method");
    check(hasRelation(pf, "Circle", "Shape", "inherits"),
          "relation Circle --inherits--> Shape");
}

void testUtils() {
    std::fprintf(stderr, "[case] utils.ts\n");
    const std::string src = readFile(fixture("utils.ts"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::TypeScriptParser p;
    const auto pf = p.parse(fixture("utils.ts"), src);

    check(findSymbol(pf, "add", "function") != nullptr, "add function");
    check(findSymbol(pf, "mul", "function") != nullptr,
          "mul function (dari arrow assignment)");
}

void testMain() {
    std::fprintf(stderr, "[case] main.ts\n");
    const std::string src = readFile(fixture("main.ts"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::TypeScriptParser p;
    const auto pf = p.parse(fixture("main.ts"), src);

    check(hasImport(pf, "./shapes"), "import './shapes'");
    check(hasRelation(pf, "main", "Circle", "calls"),
          "main --calls--> Circle (constructor)");
    check(hasRelation(pf, "main", "area", "calls"),
          "main --calls--> area");
}

void testNested() {
    std::fprintf(stderr, "[case] nested.ts\n");
    const std::string src = readFile(fixture("nested.ts"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::TypeScriptParser p;
    const auto pf = p.parse(fixture("nested.ts"), src);

    check(findSymbol(pf, "outer", "function") != nullptr, "outer function");
    check(findSymbol(pf, "outer.inner", "function") != nullptr,
          "outer.inner nested function");
}

void testTsxVariant() {
    std::fprintf(stderr, "[case] tsx/Button.tsx (variant TSX)\n");
    const std::string src = readFile(fixture("tsx/Button.tsx"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::TypeScriptParser p(
        frans::code_intelligence::TypeScriptParser::Variant::Tsx);
    const auto pf = p.parse(fixture("tsx/Button.tsx"), src);

    check(findSymbol(pf, "Button", "function") != nullptr,
          "Button function (grammar TSX, tanpa ERROR JSX)");
}

void testBroken() {
    std::fprintf(stderr, "[case] broken.ts (toleransi syntax error)\n");
    const std::string src = readFile(fixture("broken.ts"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::TypeScriptParser p;
    const auto pf = p.parse(fixture("broken.ts"), src);

    check(findSymbol(pf, "valid", "function") != nullptr,
          "valid function tetap ter-ekstrak di file broken");
}

void testEmptyInput() {
    std::fprintf(stderr, "[case] empty input\n");
    frans::code_intelligence::TypeScriptParser p;
    const auto pf = p.parse("empty.ts", "");
    check(pf.snippets.empty(), "empty source → snippets kosong");
    check(pf.relations.empty(), "empty source → relations kosong");
    check(pf.imports.empty(), "empty source → imports kosong");
}

void testDeterminism() {
    std::fprintf(stderr, "[case] determinism\n");
    const std::string src = readFile(fixture("shapes.ts"));
    if (src.empty()) { ++g_failures; return; }

    frans::code_intelligence::TypeScriptParser p;
    const auto a = p.parse(fixture("shapes.ts"), src);
    const auto b = p.parse(fixture("shapes.ts"), src);

    bool same = (a.snippets.size() == b.snippets.size()
              && a.relations.size() == b.relations.size()
              && a.imports.size() == b.imports.size());

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
    std::fprintf(stderr, "=== frans_ts_parser_test ===\n");

    testShapes();
    testUtils();
    testMain();
    testNested();
    testTsxVariant();
    testBroken();
    testEmptyInput();
    testDeterminism();

    std::fprintf(stderr,
        "\n=== %d checks, %d failures ===\n", g_checks, g_failures);
    std::fprintf(stderr, "total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
