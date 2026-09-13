// ~/AgenAI/frans/src/infrastructure/llama/LlamaCppProvider.h
#pragma once

#include "domain/llm/ILlmProvider.h"

#include <string>

namespace frans::infrastructure {

// Blueprint §5.3 — Adapter untuk llama-server (HTTP OpenAI-compatible).
//
// Endpoint yang dipakai:
//   POST http://host:port/v1/chat/completions   (stream=true → SSE)
//   GET  http://host:port/health                 (isAvailable)
//
// Cost selalu 0 karena inferensi lokal.
class LlamaCppProvider : public frans::domain::ILlmProvider {
public:
    struct Config {
        std::string host                 = "127.0.0.1";
        int         port                 = 8080;
        int         connect_timeout_sec  = 5;
        int         read_timeout_sec     = 600;   // streaming panjang: 10 menit
        std::string model_tag;                     // label "model" pada payload
    };

    explicit LlamaCppProvider(Config cfg);

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

private:
    std::string buildPayload(const std::vector<frans::domain::ChatMessage>& messages,
                             const frans::domain::GenOptions& opts,
                             bool stream) const;

    Config cfg_;
};

} // namespace frans::infrastructure
