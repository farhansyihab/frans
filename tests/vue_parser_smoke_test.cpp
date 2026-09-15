// ~/AgenAI/frans/tests/vue_parser_smoke_test.cpp
//
// Stage 10c — Vue SFC parser smoke test.
//
// Verifikasi:
//   1. <script lang="ts"> didelegasi ke TS parser + line adjust.
//   2. <script> (default) didelegasi ke JS parser.
//   3. File .vue tanpa <script> → ParsedFile kosong (tidak crash).
//   4. <script> yang tidak di posisi "standar" → line adjust benar
//      (dihitung dari posisi aktual, bukan asumsi fixed offset).
//   5. <script lang="tsx"> → grammar TSX dipakai (JSX ok).
//   6. Malformed SFC (unterminated) → kosong + no crash.
//   7. Determinism — input sama → output identik.
//   8. Imports di-propagate dari TS/JS ke ParsedFile Vue.
//   9. file_path tetap .vue (bukan .vue.ts / dst).

#include "code_intelligence/parsers/VueParser.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace frans::code_intelligence;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const std::string& label) {
    if (cond) {
        ++g_pass;
        std::printf("  PASS  %s\n", label.c_str());
    } else {
        ++g_fail;
        std::printf("  FAIL  %s\n", label.c_str());
    }
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

const CodeSnippet* findSnippet(const ParsedFile& pf,
                                const std::string& name) {
    for (const auto& s : pf.snippets) {
        if (s.symbol_name == name) return &s;
    }
    return nullptr;
}

bool hasImport(const ParsedFile& pf, const std::string& mod) {
    for (const auto& imp : pf.imports) {
        if (imp == mod) return true;
    }
    return false;
}

void section(const char* name) {
    std::printf("\n[%s]\n", name);
}

} // namespace

int main() {
    const std::string dir = FIXTURE_DIR;

    std::printf("=== frans vue parser smoke test ===\n");
    std::printf("fixture dir: %s\n", dir.c_str());

    // -----------------------------------------------------------------
    // 1. Button.vue — <script setup lang="ts">
    // -----------------------------------------------------------------
    section("Button.vue  (<script setup lang=\"ts\">)");
    {
        VueParser parser;
        const std::string src = readFile(dir + "/Button.vue");
        check(!src.empty(), "fixture readable");

        const ParsedFile pf = parser.parse("Button.vue", src);

        check(pf.file_path == "Button.vue",
              "file_path preserved (.vue, bukan .vue.ts)");

        const CodeSnippet* hc = findSnippet(pf, "handleClick");
        check(hc != nullptr, "snippet 'handleClick' found");
        if (hc) {
            check(hc->kind == "function", "handleClick kind == function");
            check(hc->file_path == "Button.vue",
                  "handleClick file_path == Button.vue");
            // Di fixture: handleClick di baris 10-12 (absolute).
            check(hc->start_line == 10,
                  "handleClick start_line == 10 (line adjust OK)");
            check(hc->end_line == 12,
                  "handleClick end_line == 12 (line adjust OK)");
        }

        check(hasImport(pf, "vue"),
              "import 'vue' propagated dari TS ke Vue");
    }

    // -----------------------------------------------------------------
    // 2. LegacyButton.vue — <script> tanpa lang (default JS)
    // -----------------------------------------------------------------
    section("LegacyButton.vue  (<script> default JS)");
    {
        VueParser parser;
        const std::string src = readFile(dir + "/LegacyButton.vue");
        check(!src.empty(), "fixture readable");

        const ParsedFile pf = parser.parse("LegacyButton.vue", src);

        const CodeSnippet* hc = findSnippet(pf, "handleClick");
        check(hc != nullptr, "snippet 'handleClick' found (JS parser)");
        if (hc) {
            check(hc->kind == "function", "handleClick kind == function");
            // Fixture: handleClick di baris 6-8.
            check(hc->start_line == 6,
                  "handleClick start_line == 6 (JS default path)");
        }
    }

    // -----------------------------------------------------------------
    // 3. NoScript.vue — tidak ada <script>
    // -----------------------------------------------------------------
    section("NoScript.vue  (no <script> block)");
    {
        VueParser parser;
        const std::string src = readFile(dir + "/NoScript.vue");
        check(!src.empty(), "fixture readable");

        const ParsedFile pf = parser.parse("NoScript.vue", src);
        check(pf.snippets.empty(),  "no snippets");
        check(pf.imports.empty(),   "no imports");
        check(pf.relations.empty(), "no relations");
    }

    // -----------------------------------------------------------------
    // 4. MixedOrder.vue — <script> tidak di posisi standar
    // -----------------------------------------------------------------
    section("MixedOrder.vue  (script setelah template + style)");
    {
        VueParser parser;
        const std::string src = readFile(dir + "/MixedOrder.vue");
        check(!src.empty(), "fixture readable");

        const ParsedFile pf = parser.parse("MixedOrder.vue", src);

        const CodeSnippet* alpha = findSnippet(pf, "alpha");
        const CodeSnippet* beta  = findSnippet(pf, "beta");
        check(alpha != nullptr, "snippet 'alpha' found");
        check(beta  != nullptr, "snippet 'beta' found");

        // Fixture: alpha di baris 10-12, beta di baris 14-16.
        if (alpha) {
            check(alpha->start_line == 10,
                  "alpha start_line == 10 (offset dari posisi aktual)");
            check(alpha->end_line   == 12,
                  "alpha end_line == 12");
        }
        if (beta) {
            check(beta->start_line == 14,
                  "beta start_line == 14");
            check(beta->end_line   == 16,
                  "beta end_line == 16");
        }
    }

    // -----------------------------------------------------------------
    // 5. TsxVue.vue — <script lang="tsx">
    // -----------------------------------------------------------------
    section("TsxVue.vue  (<script lang=\"tsx\">)");
    {
        VueParser parser;
        const std::string src = readFile(dir + "/TsxVue.vue");
        check(!src.empty(), "fixture readable");

        const ParsedFile pf = parser.parse("TsxVue.vue", src);
        check(!pf.snippets.empty(),
              "TSX variant parsed (>=1 snippet, JSX tidak bikin error)");

        const CodeSnippet* rt = findSnippet(pf, "renderTemplate");
        check(rt != nullptr, "snippet 'renderTemplate' found");
        if (rt) {
            // Fixture: renderTemplate di baris 10-12.
            check(rt->start_line == 10,
                  "renderTemplate start_line == 10");
        }

        check(hasImport(pf, "vue"), "import 'vue' propagated (TSX)");
    }

    // -----------------------------------------------------------------
    // 6. Malformed SFC — unterminated <script>
    // -----------------------------------------------------------------
    section("Malformed SFC");
    {
        VueParser parser;
        const std::string src =
            "<template></template>\n"
            "<script lang=\"ts\">\n"
            "function broken() {}\n";
        // Sengaja tanpa </script>.
        const ParsedFile pf = parser.parse("broken.vue", src);
        check(pf.snippets.empty(),
              "empty ParsedFile untuk SFC malformed");
    }

    // -----------------------------------------------------------------
    // 7. Determinism
    // -----------------------------------------------------------------
    section("Determinism");
    {
        VueParser parser;
        const std::string src = readFile(dir + "/Button.vue");

        const ParsedFile a = parser.parse("Button.vue", src);
        const ParsedFile b = parser.parse("Button.vue", src);

        check(a.snippets.size() == b.snippets.size(),
              "same snippet count");

        bool same = true;
        if (a.snippets.size() != b.snippets.size()) {
            same = false;
        } else {
            for (std::size_t i = 0; i < a.snippets.size(); ++i) {
                if (a.snippets[i].symbol_name != b.snippets[i].symbol_name
                 || a.snippets[i].start_line  != b.snippets[i].start_line
                 || a.snippets[i].end_line    != b.snippets[i].end_line
                 || a.snippets[i].content     != b.snippets[i].content) {
                    same = false;
                    break;
                }
            }
        }
        check(same, "identical output untuk input identik");
    }

    // -----------------------------------------------------------------
    // Summary
    // -----------------------------------------------------------------
    std::printf("\n=== total: %d pass, %d fail ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
