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
//
// Stage 12d: tambah `rate_limited`. Provider (Gemini, DeepSeek) set ini
// true saat server merespons dengan HTTP 429 (rate limit) atau HTTP 503
// (high demand / temporarily unavailable). Agent loop membaca flag ini
// untuk mengubah strategi eskalasi: rate limit TIDAK BOLEH memicu
// kenaikan ke tier berbayar berikutnya (blueprint §10) — malah harus
// fallback ke Local sambil menunggu cooldown.
struct GenResult {
    bool        success           = false;
    std::string text;
    int         prompt_tokens     = 0;
    int         completion_tokens = 0;
    double      elapsed_sec       = 0.0;
    double      cost_usd          = 0.0;   // Stage 8 — 0.0 untuk provider lokal
    std::string error;   // terisi kalau success == false

    // Stage 12d — true kalau kegagalan disebabkan rate limit / capacity
    // (HTTP 429 / 503). Agent loop memakai ini untuk memaksa fallback
    // ke Local, bukan naik ke paid tier berikutnya.
    bool        rate_limited      = false;
};

using TokenCallback = std::function<void(const std::string& token)>;

// Blueprint §5.3 — Strategy. Tiap backend AI (lokal, DeepSeek, Gemini)
// mengimplementasikan antarmuka ini.
class ILlmProvider {
public:
    virtual ~ILlmProvider() = default;

    virtual std::string name() const = 0;

    virtual bool isAvailable() const = 0;

    virtual GenResult generate(const std::vector<ChatMessage>& messages,
                               const GenOptions& opts) = 0;

    virtual GenResult streamGenerate(const std::vector<ChatMessage>& messages,
                                     const GenOptions& opts,
                                     TokenCallback onToken,
                                     const std::atomic<bool>* cancel_flag) = 0;

    virtual CostEstimate estimateCost(const std::vector<ChatMessage>& messages) const = 0;
};

} // namespace frans::domain