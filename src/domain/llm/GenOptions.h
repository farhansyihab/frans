// ~/AgenAI/frans/src/domain/llm/GenOptions.h
#pragma once

#include <string>
#include <vector>

namespace frans::domain {

// Satu pesan dalam percakapan. Cocok dengan format OpenAI-compatible
// (llama-server /v1/chat/completions, DeepSeek, Gemini-compat).
struct ChatMessage {
    std::string role;     // "system" | "user" | "assistant"
    std::string content;
};

// Parameter generasi. Default konservatif untuk model 7B di laptop.
// max_tokens=0 berarti serahkan ke server (tidak dikirim ke payload).
struct GenOptions {
    int         max_tokens     = 1024;
    double      temperature    = 0.7;
    double      top_p          = 0.9;
    int         top_k          = 40;
    double      repeat_penalty = 1.1;
    std::string stop_sequence;      // kosong = tidak ada
};

// Estimasi biaya API. Untuk provider lokal selalu nol.
struct CostEstimate {
    double input_usd  = 0.0;
    double output_usd = 0.0;

    double total_usd() const { return input_usd + output_usd; }
    bool   is_free()   const { return input_usd == 0.0 && output_usd == 0.0; }
};

} // namespace frans::domain
