#include "application/context/ContextSelector.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace frans::application {

namespace fs = std::filesystem;

// Helper: Estimasi token kasar (1 token ~ 4 bytes)
static std::size_t estimateTokens(std::size_t bytes) {
    return bytes / 4;
}

// Helper: Baca sebagian awal file untuk mode Index
static std::string readFileHead(const std::string& full_path, int lines) {
    std::ifstream f(full_path);
    if (!f.is_open()) return "";

    std::string line;
    std::ostringstream oss;
    int count = 0;
    while (count < lines && std::getline(f, line)) {
        oss << line << "\n";
        ++count;
    }
    return oss.str();
}

// Helper: Bangun string repo context untuk Gemini
// Mode Full: Tree + Isi semua file teks
// Mode Index: Tree + Ukuran + Head N baris per file
static std::string buildRepoContext(const CollectResult& collected,
                                    const std::string& project_root,
                                    bool is_full_mode,
                                    int index_head_lines) {
    std::ostringstream oss;

    // 1. Header sederhana
    oss << "Directory structure:\n";
    for (const auto& cf : collected.files) {
        if (cf.skipped) continue; // Jangan tampilkan secret/binary di tree
        oss << "├── " << cf.relative_path << "\n";
    }
    oss << "\n";

    // 2. File Contents
    if (is_full_mode) {
        // Kirim isi lengkap
        for (const auto& cf : collected.files) {
            if (cf.skipped || !cf.is_text) continue;

            std::string full_path = (fs::path(project_root) / cf.relative_path).string();
            std::ifstream f(full_path);
            if (!f.is_open()) continue;

            std::stringstream buffer;
            buffer << f.rdbuf();
            std::string content = buffer.str();

            oss << "================================================\n";
            oss << "FILE: " << cf.relative_path << "\n";
            oss << "================================================\n";
            oss << content << "\n\n";
        }
    } else {
        // Mode Index: Hanya head lines
        for (const auto& cf : collected.files) {
            if (cf.skipped || !cf.is_text) continue;

            std::string full_path = (fs::path(project_root) / cf.relative_path).string();
            std::string head = readFileHead(full_path, index_head_lines);

            oss << "================================================\n";
            oss << "FILE: " << cf.relative_path << " (" << cf.size_bytes << " bytes)\n";
            oss << "================================================\n";
            oss << head << "\n";
            if (cf.size_bytes > head.size()) {
                oss << "... [truncated] ...\n";
            }
            oss << "\n";
        }
    }

    return oss.str();
}

SelectionResult selectContext(const ContextRequest& req,
                              GeminiClient& client,
                              std::atomic<bool>* cancel) {
    SelectionResult result;
    result.ok = false;

    // 1. Cek Cancel di awal
    if (cancel && cancel->load()) {
        result.error = "Dibatalkan oleh pengguna sebelum mulai.";
        return result;
    }

    // 2. Kumpulkan file dari disk
    CollectOptions col_opt;
    col_opt.max_file_bytes = 256 * 1024; // Hard limit dari blueprint default
    col_opt.exclude_secrets = true;

    CollectResult collected = collectFiles(req.project_root, col_opt, cancel);

    if (!collected.ok) {
        result.error = "Gagal mengumpulkan file: " + collected.error;
        return result;
    }

    if (cancel && cancel->load()) {
        result.error = "Dibatalkan saat mengumpulkan file.";
        return result;
    }

    // Filter hanya file teks yang tidak di-skip untuk pertimbangan mode
    std::vector<const CollectedFile*> eligible_files;
    std::size_t total_eligible_bytes = 0;

    for (const auto& cf : collected.files) {
        if (!cf.skipped && cf.is_text) {
            eligible_files.push_back(&cf);
            total_eligible_bytes += cf.size_bytes;
        }
    }

    if (eligible_files.empty()) {
        result.error = "Tidak ada file teks yang layak untuk dianalisis.";
        return result;
    }

    // 3. Tentukan Mode (Full vs Index)
    std::size_t est_tokens_full = estimateTokens(total_eligible_bytes);

    bool use_full_mode = (est_tokens_full <= req.gemini_full_mode_max_tokens);
    result.mode = use_full_mode ? "full" : "index";

    std::string repo_context = buildRepoContext(collected, req.project_root,
                                                use_full_mode, req.gemini_index_head_lines);

    result.sent_bytes = repo_context.size();
    result.est_tokens = estimateTokens(result.sent_bytes);

    // 4. Siapkan Request untuk Gemini
    GeminiRequest gem_req;
    gem_req.model = req.gemini_model;
    gem_req.endpoint = req.gemini_endpoint;
    gem_req.api_key = req.api_key;
    gem_req.timeout_sec = req.gemini_timeout_sec;
    gem_req.user_request = req.request_text;
    gem_req.repo_context = repo_context;
    gem_req.selector_prompt = req.selector_prompt;

    // 5. Panggil Gemini Client
    GeminiResult gem_res = client.select(gem_req, cancel);

    if (cancel && cancel->load()) {
        result.error = "Dibatalkan selama panggilan Gemini.";
        return result;
    }

    if (!gem_res.ok) {
        result.error = "Gemini Error: " + gem_res.error.message;
        return result;
    }

    // Salin data dasar
    result.new_files_expected = gem_res.selection.new_files_expected;
    result.notes = gem_res.selection.notes;

    // 6. Validasi & Pemrosesan Hasil
    // Buat map path -> CollectedFile* untuk lookup cepat
    std::unordered_map<std::string, const CollectedFile*> file_map;
    for (const auto& cf : collected.files) {
        file_map[cf.relative_path] = &cf;
    }

    // Set untuk deduplikasi
    std::unordered_set<std::string> seen_paths;

    std::vector<ContextFileEntry> valid_candidates;

    // Proses file dari Gemini (tipe: GeminiClient::SelectedFile -> ContextFileEntry)
    for (const auto& sf : gem_res.selection.files) {
        std::string path = sf.path;

        // Normalisasi path (hapus leading ./ jika ada, pastikan forward slash)
        if (path.size() >= 2 && path[0] == '.' && path[1] == '/') {
            path.erase(0, 2);
        }

        // Cek duplikat
        if (seen_paths.count(path)) continue;

        // Validasi: Apakah path ada di daftar file nyata (collectFiles)?
        auto it = file_map.find(path);
        if (it == file_map.end()) {
            result.dropped_invalid.push_back(path);
            continue;
        }

        const CollectedFile* real_file = it->second;

        // Validasi: Apakah file tersebut boleh dibaca? (Bukan secret/binary/skipped)
        if (real_file->skipped || !real_file->is_text) {
            result.dropped_invalid.push_back(path + " (secret/binary)");
            continue;
        }

        // Aman, tambahkan ke kandidat
        ContextFileEntry sel;
        sel.path = path;
        sel.priority = sf.priority; // "must" or "helpful"
        sel.reason = sf.reason;
        sel.size_bytes = real_file->size_bytes;
        sel.selected = true;

        valid_candidates.push_back(sel);
        seen_paths.insert(path);
    }

    // 7. Tambahkan always_include (Pinned)
    for (const auto& inc_path : req.always_include) {
        std::string norm_inc = inc_path;
        if (norm_inc.size() >= 2 && norm_inc[0] == '.' && norm_inc[1] == '/') {
            norm_inc.erase(0, 2);
        }

        if (seen_paths.count(norm_inc)) continue; // Sudah dipilih Gemini

        auto it = file_map.find(norm_inc);
        if (it != file_map.end()) {
            const CollectedFile* real_file = it->second;
            if (!real_file->skipped && real_file->is_text) {
                ContextFileEntry sel;
                sel.path = norm_inc;
                sel.priority = "pinned";
                sel.reason = "Termasuk dalam always_include";
                sel.size_bytes = real_file->size_bytes;
                sel.selected = true;

                valid_candidates.push_back(sel);
                seen_paths.insert(norm_inc);
            }
        }
    }

    // 8. Terapkan Batas (Budgeting)
    // Prioritas: must/pinned > helpful
    // Urutkan untuk kemudahan filtering:
    // 1. pinned/must (tetap)
    // 2. helpful (dibuang jika melebihi batas)

    std::sort(valid_candidates.begin(), valid_candidates.end(),
              [](const ContextFileEntry& a, const ContextFileEntry& b) {
        auto prio_rank = [](const std::string& p) {
            if (p == "pinned") return 0;
            if (p == "must") return 1;
            if (p == "helpful") return 2;
            return 3;
        };
        int ra = prio_rank(a.priority);
        int rb = prio_rank(b.priority);
        if (ra != rb) return ra < rb;
        // Dalam prioritas sama, urut leksikografis untuk determinisme
        return a.path < b.path;
    });

    std::size_t current_total_bytes = 0;
    std::size_t current_count = 0;

    for (auto& cand : valid_candidates) {
        bool is_must_or_pinned = (cand.priority == "must" || cand.priority == "pinned");

        // Cek batas jumlah
        if (current_count >= req.max_files) {
            if (is_must_or_pinned) {
                // Blueprint: "File must dan pinned tidak dibuang diam-diam;
                // jika tetap melebihi batas, UI memperingatkan."
                cand.selected = true;
            } else {
                cand.selected = false;
                result.dropped_budget.push_back(cand.path);
                continue;
            }
        }

        // Cek batas ukuran
        if (current_total_bytes + cand.size_bytes > req.max_context_bytes) {
            if (is_must_or_pinned) {
                cand.selected = true;
            } else {
                cand.selected = false;
                result.dropped_budget.push_back(cand.path);
                continue;
            }
        }

        // Jika lolos atau wajib, update counter
        if (cand.selected) {
            current_total_bytes += cand.size_bytes;
            current_count++;
            result.files.push_back(cand);
        }
    }

    result.ok = true;
    return result;
}

} // namespace frans::application