// ~/AgenAI/frans/src/infrastructure/api_providers/DeepSeekProvider.h
#pragma once

#include "config/DeepSeekConfig.h"
#include "domain/llm/ILlmProvider.h"

#include <string>

namespace frans::infrastructure {

// Blueprint §5.3 — Adapter untuk DeepSeek API (OpenAI-compatible).
//
// Endpoint default: https://api.deepseek.com/chat/completions
// Model default   : "deepseek-chat" (nama model ada di providers.json)
//
// Kompatibel dengan llama.cpp karena sama-sama mengikuti skema
// /chat/completions OpenAI. Perbedaan utama:
//   - HTTPS + Bearer token (Authorization header)
//   - Server MELAPORKAN `usage` (prompt_tokens / completion_tokens) di
//     akhir stream — dipakai untuk menghitung cost_usd aktual
//   - Bisa rate-limit / error 4xx-5xx — pesan error disalin ke GenResult.error
//
// Biaya dihitung dari cfg.input_cost_per_mtok_usd & output_cost_per_mtok_usd,
// diisi ke GenResult.cost_usd setelah generation selesai. Provider lokal
// (LlamaCppProvider) selalu 0.
class DeepSeekProvider : public frans::domain::ILlmProvider {
public:
    // Konstruktor menerima DeepSeekConfig yang sudah di-resolve (termasuk
    // api_key). Kalau api_key kosong, isAvailable() return false dan
    // generate() fail dengan pesan jelas — TIDAK throw.
    explicit DeepSeekProvider(frans::config::DeepSeekConfig cfg);

    std::string name() const override;
    bool        isAvailable() const override;

    frans::domain::GenResult generate(
        const std::vector<frans::domain::ChatMessage>& messages,
        const frans::domain::GenOptions& opts) override;

    frans::domain::GenResult streamGenerate(
        const std::vector<frans::domain::ChatMessage>& messages,
        const frans::domain::GenOptions& opts,
        frans::domain::TokenCallback onToken,
        const std::atomic<bool>* cancel_flag) override;

    frans::domain::CostEstimate estimateCost(
        const std::vector<frans::domain::ChatMessage>& messages) const override;

    // Hitung biaya AKTUAL dari usage yang dilaporkan server.
    // Dipakai setelah generate/streamGenerate selesai untuk mengisi
    // GenResult.cost_usd. Ekspos publik supaya Stage 9 (DailyBudgetGuard)
    // bisa memakainya juga kalau perlu verifikasi.
    frans::domain::CostEstimate computeActualCost(int prompt_tokens,
                                                   int completion_tokens) const;

    // -------------------------------------------------------------------
    // URL parser — dipisah sebagai static publik untuk testabilitas.
    // -------------------------------------------------------------------
    struct ParsedUrl {
        bool        is_https = false;
        bool        valid    = false;
        std::string host;
        int         port     = 0;
        std::string path;    // termasuk leading '/'

        std::string str() const;   // untuk logging / error message
    };

    // Parse "https://host[:port]/path[?query]" → ParsedUrl.
    // Kalau tidak ada skema atau host kosong, valid=false.
    static ParsedUrl parseEndpoint(const std::string& url);

private:
    std::string buildPayload(
        const std::vector<frans::domain::ChatMessage>& messages,
        const frans::domain::GenOptions& opts,
        bool stream) const;

    frans::config::DeepSeekConfig cfg_;
    ParsedUrl                     parsed_endpoint_;
};

} // namespace frans::infrastructure
