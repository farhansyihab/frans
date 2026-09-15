// ~/AgenAI/frans/src/config/GeminiConfig.h
#pragma once

#include <string>

namespace frans::config {

// Stage 12 — Konfigurasi Gemini dari config/providers.json blok "gemini".
//
// Gemini adalah API GRATIS (rate-limited ketat) — kandidat tier MENENGAH
// antara local (LlamaCppProvider) dan DeepSeek (berbayar).
//
// STATUS DEFAULT (per 2025-Q4): DISABLED.
//   Free tier Gemini sering kena HTTP 503 ("high demand") / 429
//   (rate limit 15 RPM) yang membuat agent loop tidak reliabel.
//   Untuk sementara, DeepSeek langsung dipakai sebagai tier paid utama.
//
//   Flag `enabled` di providers.json mengontrol status. Set ke `true`
//   kalau ingin mengaktifkan PaidLight untuk sesi tertentu.
//
// THINKING MODE (setara think=false di Ollama):
//   Gemini 2.5 Flash / 3.x adalah thinking models — mereka menghabiskan
//   output token untuk reasoning internal dulu sebelum jawaban. Kalau
//   max_tokens kecil (mis. 32), seluruh budget bisa habis untuk thinking
//   dan completion_tokens=0, finishReason=MAX_TOKENS.
//
//   Field `thinking_budget` di providers.json mengontrol ini:
//     -1  = dynamic/auto (DEFAULT — Google yang tentukan, biasanya
//           aktif untuk task kompleks)
//      0  = disable thinking (setara think=false Ollama)
//     N>0 = budget thinking eksplisit dalam token
//
//   CATATAN: Gemini 2.5 **Pro** tidak bisa disable thinking — minimum
//   128 token. Kalau alias mengarah ke Pro, thinkingBudget=0 diabaikan
//   Google (bukan error). Untuk Gemini 3.x, API memakai `thinkingLevel`
//   (string: minimal/low/medium/high) — kita tetap kirim `thinkingBudget`
//   (number) yang backward-compatible untuk 2.5.
//
// API key TIDAK dibaca dari providers.json — hanya dari:
//   1. Env var bernama `api_key_env` (mis. GEMINI_API_KEY), atau
//   2. ~/.config/frans/secrets.json dengan key "gemini_api_key"
//
// Blueprint §8: jangan commit API key plaintext ke repo.
//
// Endpoint format: {endpoint}/models/{model}:generateContent
//                  {endpoint}/models/{model}:streamGenerateContent?alt=sse
// Auth: header `x-goog-api-key: {api_key}` (bukan Bearer token).
struct GeminiConfig {
    // Stage 12 follow-up — flag on/off. Default false (fail-safe).
    bool enabled = false;

    // Wajib ada (dari providers.json):
    std::string endpoint;        // base URL
    std::string model;           // mis. "gemini-flash-latest" (alias)
    std::string api_key_env;     // NAMA env var, mis. "GEMINI_API_KEY"

    // Thinking budget (token). -1 = auto/dynamic (default), 0 = disable.
    // Positif = budget eksplisit.
    int thinking_budget = -1;

    // Gemini gratis, tapi tetap kita simpan cost "seandainya bayar" untuk
    // audit. Router TIDAK memakai angka ini untuk budget check.
    double input_cost_per_mtok_usd  = 0.0;
    double output_cost_per_mtok_usd = 0.0;

    // Opsional (default kalau tidak ada di JSON):
    int connect_timeout_sec = 10;
    int read_timeout_sec    = 300;

    // Resolved at load time:
    std::string api_key;         // kosong kalau tidak ada env var / file

    bool hasApiKey() const { return !api_key.empty(); }

    static GeminiConfig loadFromFile(const std::string& path);
    static std::string defaultSecretsPath();
};

} // namespace frans::config