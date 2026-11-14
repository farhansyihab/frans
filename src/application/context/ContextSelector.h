#pragma once
// Fase C #59 -- ContextSelector (BluePrint v9 Addendum §4.5)
//
// Tugas: Menjembatani FileCollector <-> GeminiClient <-> SourceCodeBuilder.
// - Susun input untuk Gemini (Full/Index mode).
// - Panggil Gemini untuk seleksi file.
// - Validasi path hasil Gemini terhadap daftar file nyata.
// - Terapkan always_include, batas ukuran, dan batas jumlah.
// - Kembalikan SelectionResult untuk ditinjau manusia.
//
// Catatan nama:
//   - `frans::application::SelectedFile`      -> milik GeminiClient (data mentah dari API).
//   - `frans::application::ContextFileEntry`  -> milik ContextSelector (diperkaya dengan
//                                               size_bytes dari disk + flag `selected` untuk UI).
//   Keduanya sengaja dibedakan agar tidak terjadi redefinition ketika ContextSelector.h
//   meng-include GeminiClient.h.

#include "application/ai/GeminiClient.h"
#include "application/ingest/FileCollector.h"
#include <atomic>
#include <string>
#include <vector>
#include <cstddef>

namespace frans::application {

struct ContextRequest {
    std::string project_root;
    std::string request_text;
    std::string error_output;                 // Opsional, ikut dikirim sebagai konteks
    std::vector<std::string> always_include;  // Dari config: file yang selalu disertakan
    std::size_t max_context_bytes = 300 * 1024; // Default 300 KB
    std::size_t max_files         = 40;       // Default 40 file

    // Konfigurasi dari AppConfig
    std::string gemini_model;
    std::string gemini_endpoint;
    std::string api_key;
    std::string selector_prompt;              // Isi config/gemini_selector_prompt.txt
    int gemini_timeout_sec = 90;
    std::size_t gemini_full_mode_max_tokens = 150000;
    int gemini_index_head_lines = 40;
};

// Versi diperkaya dari GeminiClient::SelectedFile.
// Ditambah: size_bytes (dari disk) + selected (state UI checkbox).
struct ContextFileEntry {
    std::string path;
    std::string priority;   // "must" | "helpful" | "pinned" | "manual"
    std::string reason;
    std::size_t size_bytes = 0;
    bool        selected   = true;   // Dicentang di UI
};

struct SelectionResult {
    bool ok = false;
    std::string error;

    std::vector<ContextFileEntry> files;
    std::vector<std::string> new_files_expected;
    std::string notes;

    std::vector<std::string> dropped_invalid;   // Path halusinasi (tidak ada di disk)
    std::vector<std::string> dropped_budget;    // Dibuang karena batas ukuran/jumlah

    std::string mode;                           // "full" | "index"
    std::size_t sent_bytes = 0;                 // Estimasi byte yang dikirim ke Gemini
    std::size_t est_tokens = 0;                 // Estimasi token (bytes / 4)
};

/**
 * @brief Seleksi konteks via Gemini API.
 *
 * @param req Parameter permintaan dan konfigurasi.
 * @param client Instance GeminiClient yang sudah dikonfigurasi.
 * @param cancel Pointer ke flag pembatalan (opsional).
 * @return SelectionResult berisi daftar file terpilih dan metadata.
 */
SelectionResult selectContext(const ContextRequest& req,
                              GeminiClient& client,
                              std::atomic<bool>* cancel = nullptr);

} // namespace frans::application