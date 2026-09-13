// ~/AgenAI/frans/src/application/context_selector/ContextSelector.h
#pragma once

#include "code_intelligence/parsers/CodeSnippet.h"

#include <cstddef>
#include <string>
#include <vector>

namespace frans::code_intelligence {
class DependencyGraph;
}

namespace frans::application {

// Hasil satu panggilan ContextSelector.
//
// `snippets` adalah payload utama; field lain disertakan untuk audit log
// (Stage 7 akan catat ini ke file) dan untuk unit test.
struct ContextSelection {
    std::vector<frans::code_intelligence::CodeSnippet> snippets;
    std::vector<std::string>                           seed_symbols;   // ranked
    std::size_t                                        tokens_used   = 0;
    std::size_t                                        tokens_budget = 0;
};

// Blueprint §5.6 — ContextSelector (Application layer).
//
// Alur:
//   1. Tokenize prompt → identifier-like tokens.
//   2. Buang stopword + prompt-word (hardcoded Stage 6).
//   3. Match token → symbol name (exact, ci, substring) → ranked seeds.
//   4. Traverse graph: seed's own snippet → 1-hop → 2-hop, isi budget.
//
// Deterministik: input sama → output sama persis.
//
// Catatan Stage 6:
//   - Relasi "uses" TIDAK ada di graph (hanya "calls" + "inherits").
//   - Snippet yang tidak fit budget di-skip (bukan dipotong).
//   - Containment (class yang memuat method-nya) dicek satu arah; kasus
//     "method dipilih dulu, lalu class-nya" bisa menghasilkan duplikasi
//     baris. Stage 7 bisa refine kalau perlu.
class ContextSelector {
public:
    // Batas seed symbol yang dipakai untuk traversal.
    static constexpr int kMaxSeeds = 5;
    // Batas hop BFS dari tiap seed (blueprint §5.6 rekomendasi 2).
    static constexpr int kMaxHops  = 2;

    // `user_intent` = prompt mentah dari user (NL).
    // `graph`        = DependencyGraph yang sudah di-build.
    // `max_token_budget` = anggaran code context (Allocation::code_context_tokens).
    //
    // Return ContextSelection; snippets mungkin kosong kalau tidak ada
    // symbol yang match atau budget terlalu kecil untuk snippet apapun.
    ContextSelection selectContext(
        const std::string& user_intent,
        const frans::code_intelligence::DependencyGraph& graph,
        int max_token_budget) const;
};

// Format snippets jadi blok teks siap-tempel ke prompt LLM:
//
//   === geometry.py (lines 9-14)  [class Circle] ===
//   class Circle(Shape):
//       ...
//
// Pakai basename (bukan full path) supaya header ringkas; kalau nanti
// butuh disambiguasi antar direktori, tambahkan prefix 1-2 komponen.
std::string formatContextForPrompt(
    const std::vector<frans::code_intelligence::CodeSnippet>& snippets);

} // namespace frans::application
