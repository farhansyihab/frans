// ~/AgenAI/frans/tests/context_smoke_test.cpp
//
// Smoke test Stage 6: ContextBudgetManager + ContextSelector.
//
// Jalankan dari mana saja — fixture di-resolve dari FIXTURE_DIR (CMake)
// atau override via argv[1].

#include "application/context_budget/ContextBudgetManager.h"
#include "application/context_selector/ContextSelector.h"
#include "code_intelligence/builder/TreeSitterGraphBuilder.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#ifndef FIXTURE_DIR
#define FIXTURE_DIR "tests/fixtures/python"
#endif

using frans::application::ContextBudgetManager;
using frans::application::ContextSelection;
using frans::application::ContextSelector;
using frans::application::formatContextForPrompt;
using frans::code_intelligence::CodeSnippet;
using frans::code_intelligence::DependencyGraph;
using frans::code_intelligence::TreeSitterGraphBuilder;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& label) {
    if (cond) {
        std::printf("  PASS  %s\n", label.c_str());
    } else {
        std::printf("  FAIL  %s\n", label.c_str());
        ++g_failures;
    }
}

bool has_seed(const ContextSelection& s, const std::string& name) {
    return std::find(s.seed_symbols.begin(), s.seed_symbols.end(), name)
           != s.seed_symbols.end();
}

bool has_symbol(const std::vector<CodeSnippet>& v, const std::string& name) {
    for (const auto& s : v) {
        if (s.symbol_name == name) return true;
    }
    return false;
}

bool selections_equal(const ContextSelection& a, const ContextSelection& b) {
    if (a.snippets.size() != b.snippets.size()) return false;
    if (a.seed_symbols != b.seed_symbols)       return false;
    if (a.tokens_used != b.tokens_used)         return false;
    for (std::size_t i = 0; i < a.snippets.size(); ++i) {
        if (a.snippets[i].file_path   != b.snippets[i].file_path)   return false;
        if (a.snippets[i].start_line  != b.snippets[i].start_line)  return false;
        if (a.snippets[i].symbol_name != b.snippets[i].symbol_name) return false;
    }
    return true;
}

void dump_selection(const ContextSelection& sel) {
    std::printf("    seeds:");
    for (const auto& s : sel.seed_symbols) std::printf(" %s", s.c_str());
    std::printf("\n");
    std::printf("    snippets (%zu):\n", sel.snippets.size());
    for (const auto& s : sel.snippets) {
        std::printf("      %s:%d-%d  %s %s\n",
                    s.file_path.c_str(), s.start_line, s.end_line,
                    s.kind.c_str(), s.symbol_name.c_str());
    }
    std::printf("    tokens_used=%zu / %zu\n",
                sel.tokens_used, sel.tokens_budget);
}

} // namespace

int main(int argc, char** argv) {
    const std::string root = (argc > 1) ? argv[1] : FIXTURE_DIR;

    std::printf("=== frans_ai context smoke test (Stage 6) ===\n");
    std::printf("fixture root: %s\n\n", root.c_str());

    // =====================================================================
    // TEST 1 — ContextBudgetManager: window 4096
    // =====================================================================
    std::printf("[Test 1] ContextBudgetManager window=4096\n");
    {
        ContextBudgetManager mgr;
        const auto a = mgr.allocate(4096);
        check(a.system_prompt_tokens         == 300,  "system=300");
        check(a.reserved_for_response_tokens == 800,  "response=800");
        check(a.code_context_tokens          == 2247, "code=2247");
        check(a.conversation_history_tokens  == 749,  "history=749");
        check(a.total()                      == 4096, "total=4096");
    }

    // =====================================================================
    // TEST 2 — ContextBudgetManager: small window proportional
    // =====================================================================
    std::printf("\n[Test 2] ContextBudgetManager window=512 (proportional)\n");
    {
        ContextBudgetManager mgr;
        const auto a = mgr.allocate(512);
        check(a.total() == 512, "total=512");
        check(a.code_context_tokens > 0, "code > 0");
    }

    // Build graph sekali untuk semua test berikutnya.
    TreeSitterGraphBuilder builder;
    builder.build(root);
    const DependencyGraph& graph = builder.graph();

    std::printf("\n[graph] files=%zu symbols=%zu edges=%zu\n",
                graph.fileCount(), graph.symbolCount(), graph.edgeCount());

    ContextSelector selector;

    // =====================================================================
    // TEST 3 — seed matching: prompt menyebut "Circle"
    // =====================================================================
    std::printf("\n[Test 3] seed match: prompt mentions Circle & describe\n");
    {
        const std::string prompt =
            "How does the Circle class work and what does describe return?";
        const auto sel = selector.selectContext(prompt, graph, 4000);
        dump_selection(sel);
        check(has_seed(sel, "Circle"),   "seed 'Circle' present");
        check(has_seed(sel, "describe"), "seed 'describe' present");
        check(has_symbol(sel.snippets, "Circle"), "snippet Circle present");
    }

    // =====================================================================
    // TEST 4 — 1-hop traversal: caller -> helper
    // =====================================================================
    std::printf("\n[Test 4] 1-hop traversal: 'fix bug in caller'\n");
    {
        const auto sel = selector.selectContext("fix bug in caller", graph, 4000);
        dump_selection(sel);
        check(has_seed(sel, "caller"),                    "seed 'caller' present");
        check(has_symbol(sel.snippets, "caller"),         "snippet 'caller' present");
        check(has_symbol(sel.snippets, "helper"),         "snippet 'helper' (1-hop) present");
    }

    // =====================================================================
    // TEST 5 — inheritance traversal: Circle -> Shape
    // =====================================================================
    std::printf("\n[Test 5] inheritance traversal: 'explain Circle hierarchy'\n");
    {
        const auto sel = selector.selectContext("explain Circle hierarchy", graph, 4000);
        dump_selection(sel);
        check(has_seed(sel, "Circle"),             "seed 'Circle' present");
        check(has_symbol(sel.snippets, "Shape"),   "snippet 'Shape' (inherits) present");
    }

    // =====================================================================
    // TEST 6 — budget enforcement
    // =====================================================================
    std::printf("\n[Test 6] budget enforcement (window besar vs kecil)\n");
    {
        const std::string prompt =
            "Circle Shape describe run caller helper";
        const auto full = selector.selectContext(prompt, graph, 4000);
        const auto tiny = selector.selectContext(prompt, graph, 20);
        dump_selection(full);
        std::printf("    ---\n");
        dump_selection(tiny);
        check(tiny.tokens_used <= 20,                       "tiny budget respected");
        check(tiny.snippets.size() < full.snippets.size(),  "tiny < full snippet count");
    }

    // =====================================================================
    // TEST 7 — empty match
    // =====================================================================
    std::printf("\n[Test 7] empty match: 'hello world'\n");
    {
        const auto sel = selector.selectContext("hello world", graph, 4000);
        check(sel.snippets.empty(),    "no snippets");
        check(sel.seed_symbols.empty(),"no seeds");
        check(sel.tokens_used == 0,    "tokens_used=0");
    }

    // =====================================================================
    // TEST 8 — determinism
    // =====================================================================
    std::printf("\n[Test 8] determinism\n");
    {
        const std::string prompt = "How does Circle describe area work?";
        const auto a = selector.selectContext(prompt, graph, 3000);
        const auto b = selector.selectContext(prompt, graph, 3000);
        check(selections_equal(a, b), "two calls with same input identical");
    }

    // =====================================================================
    // TEST 9 — formatContextForPrompt sanity
    // =====================================================================
    std::printf("\n[Test 9] formatContextForPrompt\n");
    {
        const auto sel = selector.selectContext("Circle", graph, 2000);
        const auto txt = formatContextForPrompt(sel.snippets);
        check(!txt.empty(),                        "formatted text non-empty");
        check(txt.find("geometry.py") != std::string::npos,
                                                   "basename present");
        check(txt.find("[class Circle]") != std::string::npos,
                                                   "header contains [class Circle]");
        std::printf("    --- formatted sample (first 200 chars) ---\n");
        std::printf("%s\n", txt.substr(0, 200).c_str());
    }

    std::printf("\n=== %s ===\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
