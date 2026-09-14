// ~/AgenAI/frans/src/infrastructure/http/SseParser.h
#pragma once

#include "domain/llm/ILlmProvider.h"   // frans::domain::TokenCallback

#include <cstddef>
#include <string>

namespace frans::infrastructure {

// Parser SSE minimal untuk endpoint OpenAI-compatible
// (llama-server /v1/chat/completions, DeepSeek /chat/completions).
//
// Format yang diproses:
//   data: {"choices":[{"delta":{"content":"Hi"}}]}\n\n
//   data: {"choices":[{"delta":{"content":" there"}}]}\n\n
//   data: {"choices":[{"delta":{},"finish_reason":"stop"}],
//          "usage":{"prompt_tokens":10,"completion_tokens":5}}\n\n
//   data: [DONE]\n\n
//
// Baris lain (komentar, kosong, non-data) diabaikan. Chunk parsial aman:
// buffer menahan sisa baris terakhir sampai '\n' berikutnya tiba.
//
// Field `reasoning_content` (dipakai DeepSeek-reasoner & OpenAI o1) SENGAJA
// TIDAK diteruskan ke callback — Stage 8 memfokuskan output ke `content`
// (jawaban final). Kalau nanti butuh menampilkan reasoning, tambahkan
// parameter flag di constructor.
class SseParser {
public:
    explicit SseParser(frans::domain::TokenCallback on_token)
        : on_token_(std::move(on_token)) {}

    SseParser(const SseParser&)            = delete;
    SseParser& operator=(const SseParser&) = delete;

    // Return false untuk abort (mis. cancel). Caller harus berhenti feed().
    bool feed(const char* data, std::size_t len);

    bool               done()              const { return done_; }
    int                prompt_tokens()     const { return prompt_tokens_; }
    int                completion_tokens() const { return completion_tokens_; }
    const std::string& finish_reason()     const { return finish_reason_; }

private:
    bool processLine(const std::string& line);

    frans::domain::TokenCallback on_token_;
    std::string                  buf_;
    bool                         done_              = false;
    int                          prompt_tokens_     = 0;
    int                          completion_tokens_ = 0;
    std::string                  finish_reason_;
};

} // namespace frans::infrastructure   <-- WAJIB: tutup namespace SEBELUM include pihak ketiga

// ---------------------------------------------------------------------------
// Implementasi (header-only supaya tidak perlu .cpp terpisah — parser ini
// kecil dan dipakai oleh <= 3 provider).
//
// PENTING: include <nlohmann/json.hpp> di sini harus berada DI LUAR namespace
// frans::infrastructure. Kalau tidak, semua definisi `std::*` di dalam header
// JSON akan masuk ke namespace kita dan merusak parser compiler.
// ---------------------------------------------------------------------------

#include <nlohmann/json.hpp>

namespace frans::infrastructure {

inline bool SseParser::feed(const char* data, std::size_t len) {
    buf_.append(data, len);

    std::size_t pos;
    while ((pos = buf_.find('\n')) != std::string::npos) {
        std::string line = buf_.substr(0, pos);
        buf_.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!processLine(line)) return false;
    }
    return true;
}

inline bool SseParser::processLine(const std::string& line) {
    if (line.empty()) return true;
    if (line.rfind("data:", 0) != 0) return true;

    std::string payload = line.substr(5);
    while (!payload.empty()
           && (payload.front() == ' ' || payload.front() == '\t')) {
        payload.erase(0, 1);
    }

    if (payload == "[DONE]") {
        done_ = true;
        return true;
    }

    try {
        const auto j = nlohmann::json::parse(payload);

        if (j.contains("choices") && j["choices"].is_array()
            && !j["choices"].empty())
        {
            const auto& c = j["choices"][0];

            // `content` saja. `reasoning_content` diabaikan (lihat komentar
            // kelas).
            if (c.contains("delta") && c["delta"].contains("content")
                && !c["delta"]["content"].is_null())
            {
                const std::string tok =
                    c["delta"]["content"].get<std::string>();
                if (!tok.empty() && on_token_) on_token_(tok);
            }

            if (c.contains("finish_reason") && !c["finish_reason"].is_null()) {
                finish_reason_ = c["finish_reason"].get<std::string>();
            }
        }

        // Usage bisa datang di chunk terpisah (beberapa server mengirim
        // chunk final tanpa delta). Handle dua-duanya.
        if (j.contains("usage") && j["usage"].is_object()) {
            if (j["usage"].contains("prompt_tokens"))
                prompt_tokens_ = j["usage"]["prompt_tokens"].get<int>();
            if (j["usage"].contains("completion_tokens"))
                completion_tokens_ =
                    j["usage"]["completion_tokens"].get<int>();
        }
    } catch (...) {
        // Baris tidak lengkap / bukan JSON valid -> abaikan, tidak fatal.
    }
    return true;
}

} // namespace frans::infrastructure
