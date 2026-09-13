// ~/AgenAI/frans/src/application/agent/TargetScopeResolver.cpp
#include "application/agent/TargetScopeResolver.h"

#include "application/context_selector/ContextSelector.h"
#include "code_intelligence/graph/DependencyGraph.h"

#include <algorithm>
#include <string>

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
        scope.primary_entry = user_target;
        scope.entry_from_user = true;
    } else if (!sel.seed_symbols.empty()) {
        // Pakai urutan seed_symbols APA ADANYA dari ContextSelector
        // (sudah deterministik). Tidak hitung ulang "skor tertinggi".
        const auto snippets = graph.findBySymbol(sel.seed_symbols[0]);
        if (!snippets.empty()) {
            scope.primary_entry = toRelative(snippets[0].file_path,
                                              project_root);
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
