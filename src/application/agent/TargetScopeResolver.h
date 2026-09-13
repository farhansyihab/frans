// ~/AgenAI/frans/src/application/agent/TargetScopeResolver.h
#pragma once

#include <string>
#include <vector>

namespace frans::code_intelligence {
class DependencyGraph;
}

namespace frans::application {

// Blueprint Stage 7f — resolusi file in scope untuk satu run agent.
struct TargetScope {
    // Entry point yang akan dijalankan, RELATIF terhadap project root.
    // Diambil dari `user_target` (prioritas tertinggi) atau auto-detect
    // dari seed symbol pertama yang match (dari ContextSelector).
    std::string              primary_entry;

    // File-file yang relevan menurut ContextSelector. Termasuk
    // primary_entry. Diurutkan alfabetis untuk determinism saat
    // ditampilkan di UI/log.
    std::vector<std::string> in_scope_files;

    // Seed symbols yang match — urutan APA ADANYA dari ContextSelector
    // (sudah deterministik lewat tie-break first_pos lalu symbol).
    std::vector<std::string> seed_symbols;

    // True kalau primary_entry berasal dari user (bukan auto-detect).
    bool                     entry_from_user = false;
};

// Blueprint Stage 7f — TargetScopeResolver.
//
// Alur:
//   1. ContextSelector::selectContext(prompt, graph, budget) → snippets.
//   2. in_scope_files = unique file_path dari snippets, urut alfabetis.
//   3. primary_entry:
//        - user_target non-kosong → pakai user_target, entry_from_user=true
//        - else seed_symbols non-kosong → file_path dari findBySymbol(seed[0])
//        - else → kosong (caller harus fail)
//
// Path dari graph (absolute) dikonversi jadi relatif terhadap
// `project_root` sebelum dikembalikan. Ini deviasi kecil dari blueprint
// (yang hanya 4 param di resolve) — diperlukan karena graph menyimpan
// absolute path, sementara AgentContext butuh relative path.
class TargetScopeResolver {
public:
    // `user_target`   = targetRelativePath dari user (boleh kosong).
    // `project_root`  = root project, dipakai untuk konversi absolute→relatif.
    // `token_budget`  = dari ContextBudgetManager::allocate(...).caller
    //                   WAJIB lewat ContextBudgetManager, tidak boleh
    //                   hardcode angka di sini.
    TargetScope resolve(const std::string& task_description,
                        const std::string& user_target,
                        const std::string& project_root,
                        const frans::code_intelligence::DependencyGraph& graph,
                        int token_budget) const;
};

} // namespace frans::application
