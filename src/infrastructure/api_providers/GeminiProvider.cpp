// ~/AgenAI/frans/src/infrastructure/api_providers/GeminiProvider.cpp
#include "infrastructure/api_providers/GeminiProvider.h"

#include "httplib.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace frans::infrastructure {

using frans::domain::ChatMessage;
using frans::domain::CostEstimate;
using frans::domain::GenOptions;
using frans::domain::GenResult;
using frans::domain::TokenCallback;

using nlohmann::json;

namespace {

constexpr double kTokensPerMillion = 1'000'000.0;
constexpr int kDefaultOutputTokenEstimate = 500;

int estimateTokensFromText(const std::string& s) {
    if (s.empty()) return 0;
    return static_cast<int>((s.size() + 2) / 3);
}

// ---------------------------------------------------------------------------
// SSE accumulator inline untuk format Gemini.
// ---------------------------------------------------------------------------
class GeminiSseAccumulator {
public:
    using TokenCb = std::function<void(const std::string&)>;

    explicit GeminiSseAccumulator(TokenCb cb) : cb_(std::move(cb)) {}

    bool feed(const char* data, size_t len) {
        buf_.append(data, len);

        size_t pos;
        while ((pos = buf_.find("\n\n")) != std::string::npos) {
            std::string event = buf_.substr(0, pos);
            buf_.erase(0, pos + 2);
            if (!processEvent(event)) return false;
        }
        return true;
    }

    const std::string& finishReason()     const { return finish_reason_; }
    int                promptTokens()     const { return prompt_tokens_; }
    int                completionTokens() const { return completion_tokens_; }
    long long          totalTokens()      const { return total_tokens_; }

private:
    bool processEvent(const std::string& raw_event) {
        std::string json_str;
        size_t start = 0;
        while (start < raw_event.size()) {
            size_t nl = raw_event.find('\n', start);
            const size_t len = (nl == std::string::npos)
                ? std::string::npos : (nl - start);
            std::string line = raw_event.substr(start, len);
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line.rfind("data: ", 0) == 0) {
                if (!json_str.empty()) json_str += '\n';
                json_str += line.substr(6);
            }
            if (nl == std::string::npos) break;
            start = nl + 1;
        }

        if (json_str.empty()) return true;

        try {
            const auto j = json::parse(json_str);

            if (j.contains("candidates") && j["candidates"].is_array()
                && !j["candidates"].empty())
            {
                const auto& c = j["candidates"][0];

                if (c.contains("content") && c["content"].is_object()) {
                    const auto& content = c["content"];
                    if (content.contains("parts")
                        && content["parts"].is_array())
                    {
                        for (const auto& p : content["parts"]) {
                            if (p.contains("text") && p["text"].is_string()) {
                                const std::string txt =
                                    p["text"].get<std::string>();
                                if (!txt.empty() && cb_) cb_(txt);
                            }
                        }
                    }
                }
                if (c.contains("finishReason")
                    && c["finishReason"].is_string())
                {
                    finish_reason_ = c["finishReason"].get<std::string>();
                }
            }

            if (j.contains("usageMetadata")
                && j["usageMetadata"].is_object())
            {
                const auto& u = j["usageMetadata"];
                if (u.contains("promptTokenCount")
                    && u["promptTokenCount"].is_number_integer())
                    prompt_tokens_ = u["promptTokenCount"].get<int>();
                if (u.contains("candidatesTokenCount")
                    && u["candidatesTokenCount"].is_number_integer())
                    completion_tokens_ =
                        u["candidatesTokenCount"].get<int>();
                if (u.contains("totalTokenCount")
                    && u["totalTokenCount"].is_number_integer())
                    total_tokens_ = u["totalTokenCount"].get<long long>();
            }
        } catch (const std::exception& e) {
            std::fprintf(stderr,
                "[frans][gemini] SSE JSON parse error: %s (payload: %.80s)\n",
                e.what(), json_str.c_str());
            std::fflush(stderr);
        }
        return true;
    }

    TokenCb     cb_;
    std::string buf_;
    std::string finish_reason_;
    int         prompt_tokens_     = 0;
    int         completion_tokens_ = 0;
    long long   total_tokens_      = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// ParsedUrl
// ---------------------------------------------------------------------------
std::string GeminiProvider::ParsedUrl::str() const {
    if (!valid) return "<invalid-url>";
    return std::string(is_https ? "https://" : "http://")
         + host + ":" + std::to_string(port) + path;
}

GeminiProvider::ParsedUrl
GeminiProvider::parseEndpoint(const std::string& url) {
    ParsedUrl r;

    std::string rest;
    if (url.rfind("https://", 0) == 0) {
        r.is_https = true;
        r.port     = 443;
        rest       = url.substr(8);
    } else if (url.rfind("http://", 0) == 0) {
        r.is_https = false;
        r.port     = 80;
        rest       = url.substr(7);
    } else {
        return r;
    }
    if (rest.empty()) return r;

    const auto slash = rest.find('/');
    std::string host_port =
        (slash == std::string::npos) ? rest : rest.substr(0, slash);
    r.path = (slash == std::string::npos) ? "/" : rest.substr(slash);

    if (host_port.empty()) return r;

    const auto colon = host_port.find(':');
    if (colon == std::string::npos) {
        r.host = host_port;
    } else {
        r.host = host_port.substr(0, colon);
        const std::string port_str = host_port.substr(colon + 1);
        if (port_str.empty()) return r;
        try {
            const int p = std::stoi(port_str);
            if (p <= 0 || p > 65535) return r;
            r.port = p;
        } catch (...) {
            return r;
        }
    }
    if (r.host.empty()) return r;
    if (r.path.empty()) r.path = "/";

    r.valid = true;
    return r;
}

std::string GeminiProvider::buildEndpoint(const std::string& base_url,
                                           const std::string& model,
                                           bool stream) {
    std::string base = base_url;
    while (!base.empty() && base.back() == '/') base.pop_back();

    return base + "/models/" + model
         + (stream ? ":streamGenerateContent?alt=sse" : ":generateContent");
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
GeminiProvider::GeminiProvider(frans::config::GeminiConfig cfg)
    : cfg_(std::move(cfg))
{
    parsed_endpoint_ = parseEndpoint(cfg_.endpoint);

    if (!cfg_.enabled) {
        std::fprintf(stderr,
            "[frans][gemini] provider disabled via providers.json "
            "(gemini.enabled=false) — isAvailable() akan return false.\n");
        std::fflush(stderr);
        return;
    }

    if (!parsed_endpoint_.valid) {
        std::fprintf(stderr,
            "[frans][gemini] PERINGATAN: endpoint tidak valid: '%s'\n",
            cfg_.endpoint.c_str());
        std::fflush(stderr);
    }
    if (!cfg_.hasApiKey()) {
        std::fprintf(stderr,
            "[frans][gemini] PERINGATAN: api_key kosong — provider "
            "tidak akan bisa mengirim request.\n");
        std::fflush(stderr);
    }

    // Log thinking mode yang aktif (bantu debugging).
    if (cfg_.thinking_budget == 0) {
        std::fprintf(stderr,
            "[frans][gemini] thinking mode: DISABLED (thinkingBudget=0)\n");
        std::fflush(stderr);
    } else if (cfg_.thinking_budget > 0) {
        std::fprintf(stderr,
            "[frans][gemini] thinking mode: budget=%d tokens\n",
            cfg_.thinking_budget);
        std::fflush(stderr);
    } else {
        std::fprintf(stderr,
            "[frans][gemini] thinking mode: AUTO/dynamic (-1)\n");
        std::fflush(stderr);
    }

#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    if (parsed_endpoint_.is_https) {
        std::fprintf(stderr,
            "[frans][gemini] PERINGATAN: build tanpa CPPHTTPLIB_OPENSSL_SUPPORT "
            "— endpoint HTTPS tidak akan bisa diakses.\n");
        std::fflush(stderr);
    }
#endif
}

std::string GeminiProvider::name() const {
    return "Gemini";
}

bool GeminiProvider::isAvailable() const {
    if (!cfg_.enabled) return false;
    return parsed_endpoint_.valid && cfg_.hasApiKey();
}

// ---------------------------------------------------------------------------
// buildPayload
// ---------------------------------------------------------------------------
std::string GeminiProvider::buildPayload(
    const std::vector<ChatMessage>& messages,
    const GenOptions& opts,
    bool /*stream*/) const
{
    json j;
    json contents = json::array();
    std::string system_text;

    for (const auto& m : messages) {
        if (m.role == "system") {
            if (!system_text.empty()) system_text += "\n\n";
            system_text += m.content;
            continue;
        }
        const std::string role = (m.role == "assistant") ? "model" : "user";

        json part;
        part["text"] = m.content;

        json parts = json::array();
        parts.push_back(std::move(part));

        json content;
        content["role"]  = role;
        content["parts"] = std::move(parts);
        contents.push_back(std::move(content));
    }

    if (!system_text.empty()) {
        json sys_part;
        sys_part["text"] = system_text;
        json sys_parts = json::array();
        sys_parts.push_back(std::move(sys_part));

        json sys_instr;
        sys_instr["parts"] = std::move(sys_parts);
        j["systemInstruction"] = std::move(sys_instr);
    }

    j["contents"] = std::move(contents);

    json gen_cfg;
    gen_cfg["temperature"] = opts.temperature;
    gen_cfg["topP"]        = opts.top_p;
    if (opts.max_tokens > 0) {
        gen_cfg["maxOutputTokens"] = opts.max_tokens;
    }
    if (!opts.stop_sequence.empty()) {
        gen_cfg["stopSequences"] = json::array({opts.stop_sequence});
    }

    // ---- Thinking control (Stage 12 follow-up) ----
    // Google API: thinkingConfig.thinkingBudget = -1 (auto) / 0 (off) / N.
    // CATATAN: Gemini 2.5 Pro menolak 0 (minimum 128) — Google akan
    // mengabaikan nilai yang tidak didukung, bukan error. Gemini 3.x
    // memakai thinkingLevel (string); API masih menerima thinkingBudget
    // (number) sebagai backward-compat.
    if (cfg_.thinking_budget >= 0) {
        json thinking_cfg;
        thinking_cfg["thinkingBudget"] = cfg_.thinking_budget;
        gen_cfg["thinkingConfig"] = std::move(thinking_cfg);
    }
    // Kalau -1, JANGAN kirim thinkingConfig — biarkan Google pakai default
    // (dynamic). Mengirim -1 eksplisit juga valid, tapi omitting lebih
    // bersih dan tidak bergantung pada interpretasi API.

    j["generationConfig"] = std::move(gen_cfg);

    return j.dump();
}

// ---------------------------------------------------------------------------
// computeAuditCost
// ---------------------------------------------------------------------------
CostEstimate GeminiProvider::computeAuditCost(int prompt_tokens,
                                                int completion_tokens) const
{
    CostEstimate c;
    c.input_usd =
        static_cast<double>(prompt_tokens) * cfg_.input_cost_per_mtok_usd
        / kTokensPerMillion;
    c.output_usd =
        static_cast<double>(completion_tokens) * cfg_.output_cost_per_mtok_usd
        / kTokensPerMillion;
    return c;
}

// ---------------------------------------------------------------------------
// generate (non-streaming)
// ---------------------------------------------------------------------------
GenResult GeminiProvider::generate(const std::vector<ChatMessage>& messages,
                                    const GenOptions& opts)
{
    GenResult result;

    if (!cfg_.enabled) {
        result.error = "Gemini: provider disabled via providers.json "
                       "(gemini.enabled=false).";
        return result;
    }
    if (!parsed_endpoint_.valid) {
        result.error = "Gemini: endpoint tidak valid: " + cfg_.endpoint;
        return result;
    }
    if (!cfg_.hasApiKey()) {
        result.error = "Gemini: api_key kosong (set env "
                     + cfg_.api_key_env + " atau secrets.json)";
        return result;
    }

    const std::string full_url =
        buildEndpoint(cfg_.endpoint, cfg_.model, /*stream=*/false);
    const ParsedUrl target = parseEndpoint(full_url);
    if (!target.valid) {
        result.error = "Gemini: URL hasil build tidak valid: " + full_url;
        return result;
    }

    const auto t0 = std::chrono::steady_clock::now();
    const std::string payload = buildPayload(messages, opts, /*stream=*/false);

    const std::string body_str =
#if defined(CPPHTTPLIB_OPENSSL_SUPPORT)
        [&]() -> std::string {
            httplib::SSLClient cli(target.host, target.port);
            cli.set_connection_timeout(cfg_.connect_timeout_sec, 0);
            cli.set_read_timeout(cfg_.read_timeout_sec, 0);
            cli.set_write_timeout(30, 0);
            cli.set_keep_alive(false);
            cli.enable_server_certificate_verification(true);

            httplib::Headers headers = {
                {"Content-Type",   "application/json"},
                {"Accept",         "application/json"},
                {"x-goog-api-key", cfg_.api_key},
            };
            const auto res = cli.Post(target.path.c_str(), headers,
                                      payload, "application/json");
            if (!res) {
                result.error = "HTTP error: "
                             + httplib::to_string(res.error());
                return std::string{};
            }
            if (res->status != 200) {
                std::string detail = res->body.substr(0, 300);
                try {
                    const auto j = json::parse(res->body);
                    if (j.contains("error") && j["error"].is_object()
                        && j["error"].contains("message"))
                    {
                        detail = j["error"]["message"].get<std::string>();
                    }
                } catch (...) {}
                result.error = "HTTP " + std::to_string(res->status)
                             + ": " + detail;
                return std::string{};
            }
            return res->body;
        }()
#else
        [&]() -> std::string {
            result.error =
                "Gemini butuh HTTPS; build tanpa CPPHTTPLIB_OPENSSL_SUPPORT. "
                "Rebuild dengan OpenSSL dev package dan -DCPPHTTPLIB_OPENSSL_SUPPORT.";
            return std::string{};
        }()
#endif
        ;

    const auto t1 = std::chrono::steady_clock::now();
    result.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();

    if (!result.error.empty()) return result;

    try {
        const auto j = json::parse(body_str);

        std::string finish_reason;
        bool        has_block_reason = false;
        std::string block_reason;

        if (j.contains("promptFeedback") && j["promptFeedback"].is_object()) {
            const auto& pf = j["promptFeedback"];
            if (pf.contains("blockReason") && pf["blockReason"].is_string()) {
                has_block_reason = true;
                block_reason     = pf["blockReason"].get<std::string>();
            }
        }

        if (j.contains("candidates") && j["candidates"].is_array()
            && !j["candidates"].empty())
        {
            const auto& c = j["candidates"][0];

            if (c.contains("content") && c["content"].is_object()) {
                const auto& content = c["content"];
                if (content.contains("parts") && content["parts"].is_array()) {
                    for (const auto& p : content["parts"]) {
                        if (p.contains("text") && p["text"].is_string()) {
                            result.text += p["text"].get<std::string>();
                        }
                    }
                }
            }
            if (c.contains("finishReason") && c["finishReason"].is_string()) {
                finish_reason = c["finishReason"].get<std::string>();
            }
        }

        if (j.contains("usageMetadata") && j["usageMetadata"].is_object()) {
            const auto& u = j["usageMetadata"];
            if (u.contains("promptTokenCount")
                && u["promptTokenCount"].is_number_integer())
                result.prompt_tokens =
                    u["promptTokenCount"].get<int>();
            if (u.contains("candidatesTokenCount")
                && u["candidatesTokenCount"].is_number_integer())
                result.completion_tokens =
                    u["candidatesTokenCount"].get<int>();
        }

        const double audit_cost =
            computeAuditCost(result.prompt_tokens,
                             result.completion_tokens).total_usd();

        if (result.text.empty()) {
            if (has_block_reason) {
                result.error = "Gemini: prompt diblokir (reason='"
                             + block_reason + "'). Periksa promptFeedback.";
            } else if (finish_reason == "SAFETY") {
                result.error =
                    "Gemini: content diblokir karena safety (finishReason="
                    "'SAFETY').";
            } else if (finish_reason == "MAX_TOKENS") {
                result.error =
                    "Gemini: output terpotong oleh maxOutputTokens sebelum "
                    "content dihasilkan. Naikkan GenOptions::max_tokens.";
            } else if (finish_reason == "RECITATION") {
                result.error = "Gemini: content diblokir karena recitation "
                               "(finishReason='RECITATION').";
            } else {
                result.error =
                    "Gemini: server mengembalikan HTTP 200 tetapi content "
                    "kosong (finishReason='" + finish_reason + "').";
            }
            std::fprintf(stderr,
                "[frans][gemini] generate EMPTY: prompt=%d tok, "
                "completion=%d tok, finish='%s', audit_cost=$%.6f, %.2fs\n",
                result.prompt_tokens, result.completion_tokens,
                finish_reason.c_str(), audit_cost, result.elapsed_sec);
            std::fflush(stderr);
            return result;
        }

        result.success  = true;
        result.cost_usd = 0.0;

        std::fprintf(stderr,
            "[frans][gemini] generate OK: prompt=%d tok, completion=%d tok, "
            "(gratis, audit=$%.6f), %.2fs\n",
            result.prompt_tokens, result.completion_tokens,
            audit_cost, result.elapsed_sec);
        std::fflush(stderr);
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    return result;
}

// ---------------------------------------------------------------------------
// streamGenerate
// ---------------------------------------------------------------------------
GenResult GeminiProvider::streamGenerate(
    const std::vector<ChatMessage>& messages,
    const GenOptions& opts,
    TokenCallback onToken,
    const std::atomic<bool>* cancel_flag)
{
    GenResult result;

    if (!cfg_.enabled) {
        result.error = "Gemini: provider disabled via providers.json "
                       "(gemini.enabled=false).";
        return result;
    }
    if (!parsed_endpoint_.valid) {
        result.error = "Gemini: endpoint tidak valid: " + cfg_.endpoint;
        return result;
    }
    if (!cfg_.hasApiKey()) {
        result.error = "Gemini: api_key kosong (set env "
                     + cfg_.api_key_env + " atau secrets.json)";
        return result;
    }

    const std::string full_url =
        buildEndpoint(cfg_.endpoint, cfg_.model, /*stream=*/true);
    const ParsedUrl target = parseEndpoint(full_url);
    if (!target.valid) {
        result.error = "Gemini: URL hasil build tidak valid: " + full_url;
        return result;
    }

    const auto t0 = std::chrono::steady_clock::now();
    const std::string payload = buildPayload(messages, opts, /*stream=*/true);

    std::string accumulated;
    std::string abort_reason;

    GeminiSseAccumulator acc([&](const std::string& tok) {
        accumulated += tok;
        if (onToken) onToken(tok);
    });

#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    result.error =
        "Gemini butuh HTTPS; build tanpa CPPHTTPLIB_OPENSSL_SUPPORT. "
        "Rebuild dengan OpenSSL dev package dan -DCPPHTTPLIB_OPENSSL_SUPPORT.";
    return result;
#else
    httplib::SSLClient cli(target.host, target.port);
    cli.set_connection_timeout(cfg_.connect_timeout_sec, 0);
    cli.set_read_timeout(cfg_.read_timeout_sec, 0);
    cli.set_write_timeout(30, 0);
    cli.set_keep_alive(false);
    cli.enable_server_certificate_verification(true);

    httplib::Request req;
    req.method = "POST";
    req.path   = target.path;
    req.set_header("Content-Type",   "application/json");
    req.set_header("Accept",         "text/event-stream");
    req.set_header("x-goog-api-key", cfg_.api_key);
    req.body = payload;

    req.content_receiver =
        [&](const char* data, size_t len, uint64_t /*offset*/,
            uint64_t /*total*/) -> bool
    {
        if (cancel_flag && cancel_flag->load()) {
            abort_reason = "cancelled by user";
            return false;
        }
        if (!acc.feed(data, len)) {
            if (abort_reason.empty()) abort_reason = "SSE parse abort";
            return false;
        }
        return true;
    };

    httplib::Response res;
    httplib::Error    err = httplib::Error::Success;
    const bool ok = cli.send(req, res, err);

    const auto t1 = std::chrono::steady_clock::now();
    result.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();

    if (!ok) {
        result.error = !abort_reason.empty()
                     ? abort_reason
                     : ("HTTP error: " + httplib::to_string(err));
        result.text  = accumulated;
        return result;
    }
    if (res.status != 200) {
        std::string detail = res.body.substr(0, 300);
        try {
            const auto j = json::parse(res.body);
            if (j.contains("error") && j["error"].is_object()
                && j["error"].contains("message"))
            {
                detail = j["error"]["message"].get<std::string>();
            }
        } catch (...) {}
        result.error = "HTTP " + std::to_string(res.status) + ": " + detail;
        result.text  = accumulated;
        return result;
    }

    result.success           = true;
    result.text              = accumulated;
    result.prompt_tokens     = acc.promptTokens();
    result.completion_tokens = acc.completionTokens();
    result.cost_usd          = 0.0;

    const double audit_cost =
        computeAuditCost(result.prompt_tokens,
                         result.completion_tokens).total_usd();

    std::fprintf(stderr,
        "[frans][gemini] stream OK: prompt=%d tok, completion=%d tok, "
        "(gratis, audit=$%.6f), %.2fs, finish='%s'\n",
        result.prompt_tokens, result.completion_tokens, audit_cost,
        result.elapsed_sec, acc.finishReason().c_str());
    std::fflush(stderr);

    return result;
#endif
}

// ---------------------------------------------------------------------------
// estimateCost (pra-generation)
// ---------------------------------------------------------------------------
CostEstimate GeminiProvider::estimateCost(
    const std::vector<ChatMessage>& messages) const
{
    int input_tokens = 0;
    for (const auto& m : messages) {
        input_tokens += estimateTokensFromText(m.content);
    }
    (void)computeAuditCost(input_tokens, kDefaultOutputTokenEstimate);
    CostEstimate c;
    c.input_usd  = 0.0;
    c.output_usd = 0.0;
    return c;
}

} // namespace frans::infrastructure