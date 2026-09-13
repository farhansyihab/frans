// ~/AgenAI/frans/tests/graph_smoke_test.cpp
//
// Smoke test untuk Stage 5: build dependency graph dari folder fixture,
// cetak ringkas, lalu jalankan beberapa query getRelatedSymbols().
//
// Jalankan dari mana saja — path fixture di-resolve dari FIXTURE_DIR
// yang di-set CMake, atau override via argv[1].

#include "code_intelligence/builder/TreeSitterGraphBuilder.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifndef FIXTURE_DIR
#define FIXTURE_DIR "tests/fixtures/python"
#endif

using frans::code_intelligence::TreeSitterGraphBuilder;

namespace {

void printRelated(const frans::code_intelligence::DependencyGraph& g,
                  const std::string& name, int hops)
{
    std::cout << "\n--- getRelatedSymbols(\"" << name << "\", "
              << hops << ") ---\n";
    auto rel = g.getRelatedSymbols(name, hops);
    if (rel.empty()) {
        std::cout << "  (kosong)\n";
        return;
    }
    for (const auto& s : rel) {
        std::cout << "  " << s.file_path << ":" << s.start_line
                  << "-" << s.end_line
                  << "  " << s.kind << " " << s.symbol_name;
        if (!s.parent_class.empty()) {
            std::cout << " (in " << s.parent_class << ")";
        }
        std::cout << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::string root = (argc > 1) ? argv[1] : FIXTURE_DIR;

    std::cout << "=== frans_ai graph smoke test ===\n";
    std::cout << "fixture root: " << root << "\n\n";

    TreeSitterGraphBuilder builder;
    builder.build(root);

    const auto& g = builder.graph();

    std::cout << "build: files=" << g.fileCount()
              << "  symbols=" << g.symbolCount()
              << "  edges=" << g.edgeCount() << "\n";

    if (!builder.errors().empty()) {
        std::cout << "\nerrors:\n";
        for (const auto& e : builder.errors()) {
            std::cout << "  " << e << "\n";
        }
    }

    g.dumpTo(std::cout);

    // Query yang harus lulus untuk fixture saat ini:
    //  - "caller" (utils.py) memanggil helper → hop 1 harus dapat helper
    //  - "Circle" mewarisi Shape → hop 1 harus dapat Shape
    //  - "run" (main.py) memanggil Circle & describe → hop 1 harus dapat keduanya
    printRelated(g, "caller", 1);
    printRelated(g, "Circle", 1);
    printRelated(g, "run",    1);

    // Determinisme: panggil dua kali, harus sama.
    auto a = g.getRelatedSymbols("run", 2);
    auto b = g.getRelatedSymbols("run", 2);
    bool same = (a.size() == b.size());
    for (std::size_t i = 0; same && i < a.size(); ++i) {
        same = (a[i].file_path   == b[i].file_path)
            && (a[i].start_line  == b[i].start_line)
            && (a[i].symbol_name == b[i].symbol_name);
    }
    std::cout << "\ndeterminism check: " << (same ? "OK" : "FAIL") << "\n";

    return 0;
}
