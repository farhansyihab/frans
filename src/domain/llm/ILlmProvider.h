// ~/AgenAI/frans/src/domain/llm/ILlmProvider.h
#pragma once

#include "domain/llm/GenOptions.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace frans::domain {

// Hasil satu panggilan generate. `text` = respons lengkap (setelah stream
// selesai); untuk streaming, `text` juga diisi penuh sebagai akumulasi.
//
// Stage 8: tambah `cost_usd` (default 0.0 — provider lokal selalu 0).
// Provider berbayar (DeepSeek) mengisi field ini setelah generation selesai
// berdasarkan usage prompt_tokens/completion_tokens yang dilaporkan server.
// Stage 9 DailyBudgetGuard akan membaca field ini untuk akumulasi biaya.
struct GenResult {
    bool        success           = false;
    std::string text;
    int         prompt_tokens     = 0;
    int         completion_tokens = 0;
    double      elapsed_sec       = 0.0;
    double      cost_usd          = 0.0;   // Stage 8 — 0.0 untuk provider lokal
    std::string error;   // terisi kalau success == false
};

// Callback per potongan token (bisa lebih dari satu karakter).
using TokenCallback = std::function<void(const std::string& token)>;

// Blueprint §5.3 — Strategy. Tiap backend AI (lokal, DeepSeek, Gemini)
// mengimplementasikan antarmuka ini. Stage 4 hanya pakai LlamaCppProvider.
class ILlmProvider {
public:
    virtual ~ILlmProvider() = default;

    // Nama pendek untuk logging & UI.
    virtual std::string name() const = 0;

    // Cek cepat apakah backend siap menerima request.
    // Untuk provider lokal, biasanya GET /health.
    virtual bool isAvailable() const = 0;

    // Non-streaming: kirim seluruh percakapan, tunggu respons penuh.
    virtual GenResult generate(const std::vector<ChatMessage>& messages,
                               const GenOptions& opts) = 0;

    // Streaming: onToken dipanggil untuk setiap potongan teks.
    // cancel_flag != nullptr -> worker harus abort loop kalau flag true.
    virtual GenResult streamGenerate(const std::vector<ChatMessage>& messages,
                                     const GenOptions& opts,
                                     TokenCallback onToken,
                                     const std::atomic<bool>* cancel_flag) = 0;

    // Estimasi biaya untuk suatu prompt. Provider lokal selalu 0.
    virtual CostEstimate estimateCost(const std::vector<ChatMessage>& messages) const = 0;
};

} // namespace frans::domain
