// ~/AgenAI/frans/src/infrastructure/api_providers/DeepSeekProvider.cpp
#include "infrastructure/api_providers/DeepSeekProvider.h"

#include "httplib.h"
#include "infrastructure/http/SseParser.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

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

// Stage 12d — HTTP status rate-limit / capacity.
bool isRateLimitStatus(int status) {
    return status == 429 || status == 503;
}

} // namespace

// ---------------------------------------------------------------------------
// ParsedUrl
// ---------------------------------------------------------------------------
std::string DeepSeekProvider::ParsedUrl::str() const {
    if (!valid) return "<invalid-url>";
    return std::string(is_https ? "https://" : "http://")
         + host + ":" + std::to_string(port) + path;
}

DeepSeekProvider::ParsedUrl
DeepSeekProvider::parseEndpoint(const std::string& url) {
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

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
DeepSeekProvider::DeepSeekProvider(frans::config::DeepSeekConfig cfg)
    : cfg_(std::move(cfg))
{
    parsed_endpoint_ = parseEndpoint(cfg_.endpoint);

    if (!parsed_endpoint_.valid) {
        std::fprintf(stderr,
            "[frans][deepseek] PERINGATAN: endpoint tidak valid: '%s'\n",
            cfg_.endpoint.c_str());
        std::fflush(stderr);
    }
    if (!cfg_.hasApiKey()) {
        std::fprintf(stderr,
            "[frans][deepseek] PERINGATAN: api_key kosong — provider "
            "tidak akan bisa mengirim request.\n");
        std::fflush(stderr);
    }
#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    if (parsed_endpoint_.is_https) {
        std::fprintf(stderr,
            "[frans][deepseek] PERINGATAN: build tanpa CPPHTTPLIB_OPENSSL_SUPPORT "
            "— endpoint HTTPS tidak akan bisa diakses.\n");
        std::fflush(stderr);
    }
#endif
}

std::string DeepSeekProvider::name() const {
    return "DeepSeek";
}

bool DeepSeekProvider::isAvailable() const {
    return parsed_endpoint_.valid && cfg_.hasApiKey();
}

// ---------------------------------------------------------------------------
// buildPayload
// ---------------------------------------------------------------------------
std::string DeepSeekProvider::buildPayload(
    const std::vector<ChatMessage>& messages,
    const GenOptions& opts,
    bool stream) const
{
    json j;
    j["messages"] = json::array();
    for (const auto& m : messages) {
        j["messages"].push_back({{"role", m.role}, {"content", m.content}});
    }

    j["model"]       = cfg_.model;
    j["stream"]      = stream;
    j["temperature"] = opts.temperature;
    j["top_p"]       = opts.top_p;

    if (opts.max_tokens > 0) {
        j["max_tokens"] = opts.max_tokens;
    }
    if (!opts.stop_sequence.empty()) {
        j["stop"] = json::array({opts.stop_sequence});
    }

    if (stream) {
        j["stream_options"] = {{"include_usage", true}};
    }

    return j.dump();
}

// ---------------------------------------------------------------------------
// computeActualCost
// ---------------------------------------------------------------------------
CostEstimate DeepSeekProvider::computeActualCost(int prompt_tokens,
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
GenResult DeepSeekProvider::generate(const std::vector<ChatMessage>& messages,
                                      const GenOptions& opts)
{
    GenResult result;

    if (!parsed_endpoint_.valid) {
        result.error = "DeepSeek: endpoint tidak valid: " + cfg_.endpoint;
        return result;
    }
    if (!cfg_.hasApiKey()) {
        result.error = "DeepSeek: api_key kosong (set env "
                     + cfg_.api_key_env + " atau secrets.json)";
        return result;
    }

    const auto t0 = std::chrono::steady_clock::now();

    const std::string payload = buildPayload(messages, opts, /*stream=*/false);

    int http_status = 0;

    const std::string body_str =
#if defined(CPPHTTPLIB_OPENSSL_SUPPORT)
        [&]() -> std::string {
            httplib::SSLClient cli(parsed_endpoint_.host, parsed_endpoint_.port);
            cli.set_connection_timeout(cfg_.connect_timeout_sec, 0);
            cli.set_read_timeout(cfg_.read_timeout_sec, 0);
            cli.set_write_timeout(30, 0);
            cli.set_keep_alive(false);
            cli.enable_server_certificate_verification(true);

            httplib::Headers headers = {
                {"Content-Type",  "application/json"},
                {"Accept",        "application/json"},
                {"Authorization", "Bearer " + cfg_.api_key},
            };
            const auto res = cli.Post(parsed_endpoint_.path.c_str(), headers,
                                      payload, "application/json");
            if (!res) {
                result.error = "HTTP error: " + httplib::to_string(res.error());
                return std::string{};
            }
            http_status = res->status;
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
                "DeepSeek butuh HTTPS; build tanpa CPPHTTPLIB_OPENSSL_SUPPORT. "
                "Rebuild dengan OpenSSL dev package dan -DCPPHTTPLIB_OPENSSL_SUPPORT.";
            return std::string{};
        }()
#endif
        ;

    const auto t1 = std::chrono::steady_clock::now();
    result.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();

    // Stage 12d — set rate_limited pada HTTP 429/503.
    if (isRateLimitStatus(http_status)) {
        result.rate_limited = true;
        std::fprintf(stderr,
            "[frans][deepseek] rate-limited (HTTP %d).\n", http_status);
        std::fflush(stderr);
    }

    if (!result.error.empty()) {
        return result;
    }

    try {
        const auto j = json::parse(body_str);

        std::string finish_reason;
        bool        has_reasoning_content = false;

        if (j.contains("choices") && j["choices"].is_array()
            && !j["choices"].empty())
        {
            const auto& c = j["choices"][0];

            if (c.contains("message")) {
                const auto& msg = c["message"];
                if (msg.contains("content") && !msg["content"].is_null()) {
                    result.text = msg["content"].get<std::string>();
                }
                if (msg.contains("reasoning_content")
                    && !msg["reasoning_content"].is_null()) {
                    has_reasoning_content = true;
                }
            }
            if (c.contains("finish_reason") && !c["finish_reason"].is_null()) {
                finish_reason = c["finish_reason"].get<std::string>();
            }
        }

        if (j.contains("usage") && j["usage"].is_object()) {
            if (j["usage"].contains("prompt_tokens"))
                result.prompt_tokens =
                    j["usage"]["prompt_tokens"].get<int>();
            if (j["usage"].contains("completion_tokens"))
                result.completion_tokens =
                    j["usage"]["completion_tokens"].get<int>();
        }

        if (result.text.empty()) {
            result.cost_usd =
                computeActualCost(result.prompt_tokens,
                                  result.completion_tokens).total_usd();

            if (finish_reason == "length") {
                result.error =
                    "DeepSeek: output terpotong oleh max_tokens sebelum "
                    "content dihasilkan (finish_reason='length'). "
                    "Naikkan GenOptions::max_tokens.";
            } else if (has_reasoning_content) {
                result.error =
                    "DeepSeek: server mengembalikan reasoning_content tetapi "
                    "content kosong. Model kemungkinan reasoning model "
                    "(mis. deepseek-reasoner). Naikkan max_tokens secara "
                    "signifikan (>=512), atau ganti model ke 'deepseek-chat' "
                    "di config/providers.json.";
            } else {
                result.error =
                    "DeepSeek: server mengembalikan HTTP 200 tetapi content "
                    "kosong tanpa alasan yang terdeteksi. "
                    "finish_reason='" + finish_reason + "'.";
            }

            std::fprintf(stderr,
                "[frans][deepseek] generate EMPTY: prompt=%d tok, "
                "completion=%d tok, finish_reason='%s', reasoning=%d, %.2fs\n",
                result.prompt_tokens, result.completion_tokens,
                finish_reason.c_str(), has_reasoning_content ? 1 : 0,
                result.elapsed_sec);
            std::fflush(stderr);
            return result;
        }

        result.success  = true;
        result.cost_usd =
            computeActualCost(result.prompt_tokens,
                              result.completion_tokens).total_usd();

        std::fprintf(stderr,
            "[frans][deepseek] generate OK: prompt=%d tok, completion=%d tok, "
            "cost=$%.6f, %.2fs\n",
            result.prompt_tokens, result.completion_tokens,
            result.cost_usd, result.elapsed_sec);
        std::fflush(stderr);
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    return result;
}

// ---------------------------------------------------------------------------
// streamGenerate
// ---------------------------------------------------------------------------
GenResult DeepSeekProvider::streamGenerate(
    const std::vector<ChatMessage>& messages,
    const GenOptions& opts,
    TokenCallback onToken,
    const std::atomic<bool>* cancel_flag)
{
    GenResult result;

    if (!parsed_endpoint_.valid) {
        result.error = "DeepSeek: endpoint tidak valid: " + cfg_.endpoint;
        return result;
    }
    if (!cfg_.hasApiKey()) {
        result.error = "DeepSeek: api_key kosong (set env "
                     + cfg_.api_key_env + " atau secrets.json)";
        return result;
    }

    const auto t0 = std::chrono::steady_clock::now();

    const std::string payload = buildPayload(messages, opts, /*stream=*/true);

    std::string accumulated;
    std::string abort_reason;

    SseParser parser([&](const std::string& tok) {
        accumulated += tok;
        if (onToken) onToken(tok);
    });

#if !defined(CPPHTTPLIB_OPENSSL_SUPPORT)
    result.error =
        "DeepSeek butuh HTTPS; build tanpa CPPHTTPLIB_OPENSSL_SUPPORT. "
        "Rebuild dengan OpenSSL dev package dan -DCPPHTTPLIB_OPENSSL_SUPPORT.";
    return result;
#else
    httplib::SSLClient cli(parsed_endpoint_.host, parsed_endpoint_.port);
    cli.set_connection_timeout(cfg_.connect_timeout_sec, 0);
    cli.set_read_timeout(cfg_.read_timeout_sec, 0);
    cli.set_write_timeout(30, 0);
    cli.set_keep_alive(false);
    cli.enable_server_certificate_verification(true);

    httplib::Request req;
    req.method = "POST";
    req.path   = parsed_endpoint_.path;
    req.set_header("Content-Type",  "application/json");
    req.set_header("Accept",        "text/event-stream");
    req.set_header("Authorization", "Bearer " + cfg_.api_key);
    req.body = payload;

    req.content_receiver =
        [&](const char* data, size_t len, uint64_t /*offset*/,
            uint64_t /*total*/) -> bool
    {
        if (cancel_flag && cancel_flag->load()) {
            abort_reason = "cancelled by user";
            return false;
        }
        if (!parser.feed(data, len)) {
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

        // Stage 12d.
        if (isRateLimitStatus(res.status)) {
            result.rate_limited = true;
            std::fprintf(stderr,
                "[frans][deepseek] stream rate-limited (HTTP %d).\n",
                res.status);
            std::fflush(stderr);
        }

        result.text  = accumulated;
        return result;
    }

    result.success           = true;
    result.text              = accumulated;
    result.prompt_tokens     = parser.prompt_tokens();
    result.completion_tokens = parser.completion_tokens();
    result.cost_usd =
        computeActualCost(result.prompt_tokens,
                          result.completion_tokens).total_usd();

    std::fprintf(stderr,
        "[frans][deepseek] stream OK: prompt=%d tok, completion=%d tok, "
        "cost=$%.6f, %.2fs, finish_reason='%s'\n",
        result.prompt_tokens, result.completion_tokens, result.cost_usd,
        result.elapsed_sec, parser.finish_reason().c_str());
    std::fflush(stderr);

    return result;
#endif
}

// ---------------------------------------------------------------------------
// estimateCost
// ---------------------------------------------------------------------------
CostEstimate DeepSeekProvider::estimateCost(
    const std::vector<ChatMessage>& messages) const
{
    int input_tokens = 0;
    for (const auto& m : messages) {
        input_tokens += estimateTokensFromText(m.content);
    }
    return computeActualCost(input_tokens, kDefaultOutputTokenEstimate);
}

} // namespace frans::infrastructure