// ~/AgenAI/frans/src/infrastructure/api_providers/GeminiProvider.h
#pragma once

#include "config/GeminiConfig.h"
#include "domain/llm/ILlmProvider.h"

#include <string>

namespace frans::infrastructure {

// Stage 12 — Adapter untuk Google Gemini API (generateContent /
// streamGenerateContent). Blueprint §5.3 tier 2 (gratis, rate-limited).
//
// Perbedaan utama dari DeepSeekProvider:
//   - Auth header:  `x-goog-api-key: <KEY>`  (bukan `Authorization: Bearer`)
//   - Path:         /v1beta/models/<model>:generateContent
//                   /v1beta/models/<model>:streamGenerateContent?alt=sse
//   - Body:         {"contents":[...], "systemInstruction":{...},
//                    "generationConfig":{...}} — bukan OpenAI-compatible
//   - Response:     candidates[0].content.parts[*].text
//                   usageMetadata.{promptTokenCount,candidatesTokenCount}
//   - Role mapping: "assistant" -> "model", "system" -> systemInstruction
//
// Gemini gratis: cost_usd selalu 0. Field cost_per_mtok di config hanya
// untuk audit (menghitung "seandainya bayar"), tidak mempengaruhi budget.
class GeminiProvider : public frans::domain::ILlmProvider {
public:
    explicit GeminiProvider(frans::config::GeminiConfig cfg);

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

    // -------------------------------------------------------------------
    // Helper static publik — dipisah untuk testabilitas.
    // -------------------------------------------------------------------
    struct ParsedUrl {
        bool        is_https = false;
        bool        valid    = false;
        std::string host;
        int         port     = 0;
        std::string path;   // termasuk leading '/' dan query string

        std::string str() const;
    };

    // Parse "https://host[:port]/path[?query]" → ParsedUrl.
    static ParsedUrl parseEndpoint(const std::string& url);

    // Bangun URL lengkap generate/stream dari base URL + model.
    //   stream=false → "<base>/models/<model>:generateContent"
    //   stream=true  → "<base>/models/<model>:streamGenerateContent?alt=sse"
    static std::string buildEndpoint(const std::string& base_url,
                                     const std::string& model,
                                     bool stream);

private:
    std::string buildPayload(
        const std::vector<frans::domain::ChatMessage>& messages,
        const frans::domain::GenOptions& opts,
        bool stream) const;

    // Hitung biaya "seandainya bayar" — untuk audit saja (Gemini gratis).
    frans::domain::CostEstimate computeAuditCost(int prompt_tokens,
                                                  int completion_tokens) const;

    frans::config::GeminiConfig cfg_;
    ParsedUrl                   parsed_endpoint_;
};

} // namespace frans::infrastructure