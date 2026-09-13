// ~/AgenAI/frans/src/application/context_selector/ContextSelector.cpp
#include "application/context_selector/ContextSelector.h"

#include "application/context_selector/TokenEstimate.h"
#include "code_intelligence/graph/DependencyGraph.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace frans::application {

using frans::code_intelligence::CodeSnippet;
using frans::code_intelligence::DependencyGraph;

namespace {

// ---------------------------------------------------------------------------
// Word lists (hardcoded Stage 6; tune nanti kalau terbukti perlu config).
// ---------------------------------------------------------------------------

const std::unordered_set<std::string>& stopwords() {
    static const std::unordered_set<std::string> kSet = {
        // Kata fungsi bahasa Inggris umum
        "the","a","an","is","are","was","were","be","been","being",
        "to","of","in","on","at","by","for","with","from","into","through",
        "and","or","but","not","no","yes","so",
        "this","that","these","those",
        "it","its","he","she","they","them","their",
        "i","you","we","us","my","your","our",
        "do","does","did","done","doing",
        "have","has","had","having",
        "can","could","will","would","shall","should","may","might","must",
        "if","then","else","when","while","where","how","what","which",
        "who","why",
    };
    return kSet;
}

const std::unordered_set<std::string>& prompt_words() {
    // Kata yang sering muncul di instruksi coding tapi hampir tidak pernah
    // jadi nama simbol. Buang supaya tidak jadi noise saat matching.
    static const std::unordered_set<std::string> kSet = {
        "function","class","method","file","code","module","project",
        "please","help","fix","refactor","bug","show","explain",
        "bugfix","feature","implement","add","remove","delete","update",
        "change","like","want","need","make","create","write","read",
    };
    return kSet;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string toLower(std::string s) {
    for (auto& c : s) {
        c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string basename(const std::string& path) {
    const auto pos = path.find_last_of('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::vector<std::string> extractIdentifiers(const std::string& prompt) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : prompt) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '_') {
            cur.push_back(c);
        } else if (!cur.empty()) {
            out.push_back(std::move(cur));
            cur.clear();
        }
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}

std::vector<std::string> filterTokens(std::vector<std::string> raw) {
    std::vector<std::string> out;
    out.reserve(raw.size());
    for (auto& t : raw) {
        if (t.size() < 2) continue;
        const std::string low = toLower(t);
        if (stopwords().count(low))    continue;
        if (prompt_words().count(low)) continue;
        out.push_back(std::move(t));
    }
    return out;
}

struct Seed {
    std::string symbol;
    int         score     = 0;
    int         first_pos = 0;
};

std::vector<Seed> rankSeeds(const std::vector<std::string>& tokens,
                            const DependencyGraph& graph) {
    // Snapshot nama simbol + lowercase-nya sekali. Nanti inner loop tinggal
    // compare string, tidak perlu toLower() per token per simbol.
    struct SymRef {
        std::string original;
        std::string lower;
    };
    std::vector<SymRef> syms;
    syms.reserve(graph.allSymbols().size());
    for (const auto& [name, _] : graph.allSymbols()) {
        syms.push_back({name, toLower(name)});
    }

    std::unordered_map<std::string, Seed> by_symbol;

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok     = tokens[i];
        const std::string  low_tok = toLower(tok);

        for (const auto& sr : syms) {
            int score = 0;
            if (sr.original == tok) {
                score = 10;
            } else if (sr.lower == low_tok) {
                score = 5;
            } else if (tok.size() >= 4
                    && sr.original.find(tok) != std::string::npos) {
                // Token adalah substring nama simbol (mis. "helper" ⊂
                // "helper_v2"). Batas 4 char untuk hindari noise pendek.
                score = 2;
            } else if (sr.original.size() >= 4
                    && tok.find(sr.original) != std::string::npos) {
                // Nama simbol adalah substring token.
                score = 1;
            }

            if (score == 0) continue;

            auto it = by_symbol.find(sr.original);
            if (it == by_symbol.end()) {
                Seed s;
                s.symbol    = sr.original;
                s.score     = score;
                s.first_pos = static_cast<int>(i);
                by_symbol.emplace(sr.original, std::move(s));
            } else {
                it->second.score += score;
                if (static_cast<int>(i) < it->second.first_pos) {
                    it->second.first_pos = static_cast<int>(i);
                }
            }
        }
    }

    std::vector<Seed> ranked;
    ranked.reserve(by_symbol.size());
    for (auto& [_, s] : by_symbol) ranked.push_back(std::move(s));

    std::sort(ranked.begin(), ranked.end(),
        [](const Seed& a, const Seed& b) {
            if (a.score     != b.score)     return a.score > b.score;
            if (a.first_pos != b.first_pos) return a.first_pos < b.first_pos;
            return a.symbol < b.symbol;
        });

    if (ranked.size() > static_cast<std::size_t>(ContextSelector::kMaxSeeds)) {
        ranked.resize(static_cast<std::size_t>(ContextSelector::kMaxSeeds));
    }
    return ranked;
}

} // namespace

// ---------------------------------------------------------------------------
// ContextSelector::selectContext
// ---------------------------------------------------------------------------

ContextSelection ContextSelector::selectContext(
    const std::string& user_intent,
    const DependencyGraph& graph,
    int max_token_budget) const
{
    ContextSelection out;
    out.tokens_budget = max_token_budget > 0
                      ? static_cast<std::size_t>(max_token_budget)
                      : 0;

    std::fprintf(stderr,
        "[frans] ContextSelector: budget=%d tokens, graph: %zu files/%zu symbols\n",
        max_token_budget, graph.fileCount(), graph.symbolCount());

    if (max_token_budget <= 0 || graph.symbolCount() == 0) {
        std::fprintf(stderr,
            "[frans] ContextSelector: budget<=0 atau graph kosong -> empty\n");
        return out;
    }

    // 1. Tokenize + filter.
    const auto raw    = extractIdentifiers(user_intent);
    const auto tokens = filterTokens(raw);

    std::fprintf(stderr, "[frans] ContextSelector: %zu token after filter:",
                 tokens.size());
    for (const auto& t : tokens) std::fprintf(stderr, " '%s'", t.c_str());
    std::fprintf(stderr, "\n");

    if (tokens.empty()) return out;

    // 2. Rank seeds.
    auto seeds = rankSeeds(tokens, graph);

    std::fprintf(stderr, "[frans] ContextSelector: %zu seed(s):",
                 seeds.size());
    for (const auto& s : seeds) {
        std::fprintf(stderr, " %s(score=%d)",
                     s.symbol.c_str(), s.score);
    }
    std::fprintf(stderr, "\n");

    if (seeds.empty()) {
        std::fprintf(stderr,
            "[frans] ContextSelector: no seed symbols matched from prompt\n");
        return out;
    }

    for (const auto& s : seeds) out.seed_symbols.push_back(s.symbol);

    // 3. Traversal + budget enforcement.
    // `seen` key: (file_path, start_line) — identitas snippet.
    std::set<std::pair<std::string, int>> seen;

    // Snippet baru dianggap "sudah terwakili" kalau ada snippet lain yang
    // menaunginya (biasanya: class → method di dalamnya).
    auto already_contained = [&](const CodeSnippet& s) -> bool {
        for (const auto& sel : out.snippets) {
            if (sel.file_path != s.file_path) continue;
            if (sel.start_line <= s.start_line
                && sel.end_line >= s.end_line) {
                return true;
            }
        }
        return false;
    };

    auto try_add = [&](const CodeSnippet& s) {
        const auto key = std::make_pair(s.file_path, s.start_line);
        if (seen.count(key)) return;

        if (already_contained(s)) {
            // Snippet ini barisnya sudah ada di snippet lain yang lebih besar.
            seen.insert(key);
            return;
        }

        const std::size_t t = estimateTokens(s.content);
        if (out.tokens_used + t > out.tokens_budget) {
            // Tidak muat. Skip — jangan potong content (potongan kode
            // setengah jalan bikin LLM bingung).
            seen.insert(key);
            return;
        }

        seen.insert(key);
        out.snippets.push_back(s);
        out.tokens_used += t;
    };

    // 3a. Snippet dari seed itu sendiri (prioritas tertinggi).
    for (const auto& seed : seeds) {
        for (const auto& s : graph.findBySymbol(seed.symbol)) {
            try_add(s);
        }
    }

    // 3b. 1-hop expansion. getRelatedSymbols sudah di-sort deterministik.
    for (const auto& seed : seeds) {
        for (const auto& s : graph.getRelatedSymbols(seed.symbol, 1)) {
            try_add(s);
        }
    }

    // 3c. 2-hop expansion — best effort, sering terpotong budget.
    for (const auto& seed : seeds) {
        for (const auto& s : graph.getRelatedSymbols(seed.symbol, kMaxHops)) {
            try_add(s);
        }
    }

    std::fprintf(stderr,
        "[frans] ContextSelector: selected %zu snippet(s), %zu / %zu tokens\n",
        out.snippets.size(), out.tokens_used, out.tokens_budget);

    return out;
}

// ---------------------------------------------------------------------------
// formatContextForPrompt
// ---------------------------------------------------------------------------

std::string formatContextForPrompt(const std::vector<CodeSnippet>& snippets) {
    std::string out;
    for (const auto& s : snippets) {
        out += "=== ";
        out += basename(s.file_path);
        out += " (lines ";
        out += std::to_string(s.start_line);
        out += "-";
        out += std::to_string(s.end_line);
        out += ")  [";
        out += s.kind;
        out += " ";
        out += s.symbol_name;
        if (!s.parent_class.empty()) {
            out += " in ";
            out += s.parent_class;
        }
        out += "] ===\n";
        out += s.content;
        if (s.content.empty() || s.content.back() != '\n') out += '\n';
        out += "\n";
    }
    return out;
}

} // namespace frans::application
