// ~/AgenAI/frans/src/code_intelligence/graph/DependencyGraph.cpp
#include "code_intelligence/graph/DependencyGraph.h"

#include <algorithm>
#include <ostream>
#include <queue>
#include <set>

namespace frans::code_intelligence {

void DependencyGraph::clear() {
    files_.clear();
    symbol_index_.clear();
    edges_.clear();
}

void DependencyGraph::addFile(const ParsedFile& parsed) {
    files_[parsed.file_path] = parsed;
    rebuildIndex();
}

void DependencyGraph::removeFile(const std::string& file_path) {
    if (files_.erase(file_path) > 0) {
        rebuildIndex();
    }
}

std::size_t DependencyGraph::edgeCount() const {
    std::size_t n = 0;
    for (const auto& [_, set] : edges_) n += set.size();
    return n;
}

void DependencyGraph::rebuildIndex() {
    symbol_index_.clear();
    edges_.clear();

    for (const auto& [path, pf] : files_) {
        for (std::size_t i = 0; i < pf.snippets.size(); ++i) {
            const auto& s = pf.snippets[i];
            symbol_index_[s.symbol_name].push_back({path, i});
        }
        for (const auto& r : pf.relations) {
            if (r.from_symbol.empty() || r.to_symbol.empty()) continue;
            edges_[r.from_symbol].insert(r.to_symbol);
        }
    }
}

std::vector<CodeSnippet> DependencyGraph::findBySymbol(
    const std::string& name) const
{
    std::vector<CodeSnippet> out;
    auto it = symbol_index_.find(name);
    if (it == symbol_index_.end()) return out;

    out.reserve(it->second.size());
    for (const auto& [path, idx] : it->second) {
        auto fit = files_.find(path);
        if (fit == files_.end()) continue;
        if (idx >= fit->second.snippets.size()) continue;
        out.push_back(fit->second.snippets[idx]);
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<CodeSnippet> DependencyGraph::getRelatedSymbols(
    const std::string& name, int maxHops) const
{
    std::vector<CodeSnippet> out;
    if (maxHops <= 0 || name.empty()) return out;

    std::set<std::string> visited;
    std::queue<std::pair<std::string, int>> frontier;

    visited.insert(name);
    frontier.push({name, 0});

    while (!frontier.empty()) {
        auto [sym, hop] = frontier.front();
        frontier.pop();
        if (hop >= maxHops) continue;

        auto eit = edges_.find(sym);
        if (eit == edges_.end()) continue;

        for (const auto& next : eit->second) {
            if (visited.count(next)) continue;
            visited.insert(next);
            frontier.push({next, hop + 1});

            // Tarik semua snippet untuk simbol `next`.
            auto sit = symbol_index_.find(next);
            if (sit == symbol_index_.end()) continue;
            for (const auto& [path, idx] : sit->second) {
                auto fit = files_.find(path);
                if (fit == files_.end()) continue;
                if (idx >= fit->second.snippets.size()) continue;
                out.push_back(fit->second.snippets[idx]);
            }
        }
    }

    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> DependencyGraph::allFilePaths() const {
    std::vector<std::string> out;
    out.reserve(files_.size());
    // std::map iterasi by key (sorted), jadi hasil sudah alfabetis.
    for (const auto& [path, _] : files_) {
        out.push_back(path);
    }
    return out;
}

void DependencyGraph::dumpTo(std::ostream& os) const {
    os << "DependencyGraph: " << fileCount() << " files, "
       << symbolCount() << " symbols, " << edgeCount() << " edges\n";

    for (const auto& [path, pf] : files_) {
        os << "\n[" << path << "]\n";
        os << "  snippets (" << pf.snippets.size() << "):\n";
        for (const auto& s : pf.snippets) {
            os << "    " << s.start_line << "-" << s.end_line
               << "  " << s.kind << " " << s.symbol_name;
            if (!s.parent_class.empty()) os << " (in " << s.parent_class << ")";
            os << "\n";
        }
        if (!pf.imports.empty()) {
            os << "  imports: ";
            for (std::size_t i = 0; i < pf.imports.size(); ++i) {
                if (i) os << ", ";
                os << pf.imports[i];
            }
            os << "\n";
        }
        if (!pf.relations.empty()) {
            os << "  relations:\n";
            for (const auto& r : pf.relations) {
                os << "    " << r.from_symbol << " --" << r.kind << "--> "
                   << r.to_symbol << "\n";
            }
        }
    }
}

} // namespace frans::code_intelligence
