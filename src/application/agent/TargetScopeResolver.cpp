// ~/AgenAI/frans/src/application/agent/TargetScopeResolver.cpp
#include "application/agent/TargetScopeResolver.h"

#include "application/context_selector/ContextSelector.h"
#include "code_intelligence/graph/DependencyGraph.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

namespace frans::application {

namespace {

// Kalau `path` berada di bawah `root`, return relative path-nya.
// Kalau tidak, return `path` apa adanya (kemungkinan caller salah,
// tapi kita jangan buang informasi).
std::string toRelative(const std::string& path, const std::string& root) {
    if (path.empty() || root.empty()) return path;

    std::string prefix = root;
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';

    if (path.rfind(prefix, 0) == 0) {
        return path.substr(prefix.size());
    }
    return path;
}

// Ambil basename dari path (komponen setelah '/' terakhir).
std::string basename(const std::string& path) {
    const auto pos = path.find_last_of('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

// Stage 7g+1 (patch) — cari nama file .py yang disebut eksplisit di
// task description dan benar-benar ada di graph.
//
// Alasan: tanpa ini, kalau task menyebut "in utils.py" tapi symbol
// target (mis. `greet`) tidak ada di graph karena state tercampur,
// ContextSelector jatuh ke seed lain yang kebetulan match, dan
// primary_entry bisa jadi file yang salah. Heuristik ini menangkap
// kasus "user menyebut nama file secara eksplisit" yang sangat umum.
//
// Strategi:
//   1. Exact match: token "X.py" cocok persis dengan path relatif di graph.
//   2. Basename match: token "X.py" cocok dengan basename sebuah file —
//      hanya dipakai kalau UNIK (1 file saja di seluruh project).
//
// Return relatif path, atau "" kalau tidak ada.
std::string extractFileHintFromTask(
    const std::string& task,
    const frans::code_intelligence::DependencyGraph& graph,
    const std::string& project_root)
{
    // Kumpulkan set file paths (relatif) di graph.
    std::set<std::string> graph_files;
    for (const auto& path : graph.allFilePaths()) {
        graph_files.insert(toRelative(path, project_root));
    }
    if (graph_files.empty()) return {};

    // Helper: cek token, return match kalau ada.
    auto try_match = [&](const std::string& tok) -> std::string {
        if (tok.size() <= 3) return {};
        if (tok.compare(tok.size() - 3, 3, ".py") != 0) return {};

        // Normalize leading "./".
        std::string cleaned = tok;
        while (cleaned.rfind("./", 0) == 0) cleaned.erase(0, 2);

        // 1. Exact path match.
        if (graph_files.count(cleaned)) return cleaned;

        // 2. Basename match — hanya kalau unik.
        const std::string base = basename(cleaned);
        std::string found;
        int count = 0;
        for (const auto& f : graph_files) {
            if (basename(f) == base) {
                found = f;
                ++count;
                if (count > 1) break;
            }
        }
        if (count == 1) return found;

        return {};
    };

    // Tokenize: filename-valid chars = alnum + _ / - .
    std::string cur;
    for (char c : task) {
        const unsigned char uc = static_cast<unsigned char>(c);
        const bool is_fn_char =
            std::isalnum(uc) || c == '_' || c == '/' || c == '-' || c == '.';
        if (is_fn_char) {
            cur.push_back(c);
        } else {
            if (!cur.empty()) {
                const std::string m = try_match(cur);
                if (!m.empty()) return m;
                cur.clear();
            }
        }
    }
    // Trailing token.
    if (!cur.empty()) {
        const std::string m = try_match(cur);
        if (!m.empty()) return m;
    }

    return {};
}

} // namespace

TargetScope TargetScopeResolver::resolve(
    const std::string& task_description,
    const std::string& user_target,
    const std::string& project_root,
    const frans::code_intelligence::DependencyGraph& graph,
    int token_budget) const
{
    TargetScope scope;

    // ---- 1. Context selection (reuse ContextSelector) ----
    ContextSelector selector;
    const auto sel = selector.selectContext(task_description, graph, token_budget);
    scope.seed_symbols = sel.seed_symbols;

    // ---- 2. In-scope files: unique file_path dari snippets ----
    std::vector<std::string> files;
    files.reserve(sel.snippets.size());
    for (const auto& s : sel.snippets) {
        files.push_back(toRelative(s.file_path, project_root));
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    scope.in_scope_files = std::move(files);

    // ---- 3. Primary entry ----
    if (!user_target.empty()) {
        // Prioritas tertinggi: user override eksplisit.
        scope.primary_entry  = user_target;
        scope.entry_from_user = true;
    } else {
        // 3a. NEW — coba tangkap nama file .py yang disebut eksplisit
        // di task description.
        const std::string hint =
            extractFileHintFromTask(task_description, graph, project_root);
        if (!hint.empty()) {
            scope.primary_entry   = hint;
            scope.entry_from_user = false;   // tetap auto-detect
        }
        // 3b. Fallback — pakai urutan seed_symbols APA ADANYA dari
        // ContextSelector (sudah deterministik). Tidak hitung ulang
        // "skor tertinggi".
        else if (!sel.seed_symbols.empty()) {
            const auto snippets = graph.findBySymbol(sel.seed_symbols[0]);
            if (!snippets.empty()) {
                scope.primary_entry = toRelative(snippets[0].file_path,
                                                  project_root);
            }
        }
    }

    // ---- 4. Pastikan primary_entry ada di in_scope_files ----
    if (!scope.primary_entry.empty()) {
        const auto it = std::find(scope.in_scope_files.begin(),
                                   scope.in_scope_files.end(),
                                   scope.primary_entry);
        if (it == scope.in_scope_files.end()) {
            scope.in_scope_files.push_back(scope.primary_entry);
            std::sort(scope.in_scope_files.begin(),
                      scope.in_scope_files.end());
        }
    }

    return scope;
}

} // namespace frans::application
