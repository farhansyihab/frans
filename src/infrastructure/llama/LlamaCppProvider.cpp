// ~/AgenAI/frans/src/infrastructure/llama/LlamaCppProvider.cpp
#include "infrastructure/llama/LlamaCppProvider.h"

#include "httplib.h"
#include "infrastructure/http/SseParser.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <utility>

namespace frans::infrastructure {

using frans::domain::ChatMessage;
using frans::domain::CostEstimate;
using frans::domain::GenOptions;
using frans::domain::GenResult;
using frans::domain::TokenCallback;

using nlohmann::json;

// ---------------------------------------------------------------------------
// LlamaCppProvider
// ---------------------------------------------------------------------------
LlamaCppProvider::LlamaCppProvider(Config cfg) : cfg_(std::move(cfg)) {}

std::string LlamaCppProvider::name() const {
    return "LlamaCpp(local)";
}

bool LlamaCppProvider::isAvailable() const {
    httplib::Client cli(cfg_.host, cfg_.port);
    cli.set_connection_timeout(1, 0);
    cli.set_read_timeout(2, 0);
    cli.set_write_timeout(1, 0);
    cli.set_keep_alive(false);

    const auto res = cli.Get("/health");
    return res && res->status == 200;
}

std::string LlamaCppProvider::buildPayload(
    const std::vector<ChatMessage>& messages,
    const GenOptions& opts,
    bool stream) const
{
    json j;
    j["messages"] = json::array();
    for (const auto& m : messages) {
        j["messages"].push_back({{"role", m.role}, {"content", m.content}});
    }

    j["stream"]         = stream;
    j["temperature"]    = opts.temperature;
    j["top_p"]          = opts.top_p;
    j["top_k"]          = opts.top_k;
    j["repeat_penalty"] = opts.repeat_penalty;

    if (opts.max_tokens > 0) {
        j["max_tokens"] = opts.max_tokens;
    }
    if (!opts.stop_sequence.empty()) {
        j["stop"] = json::array({opts.stop_sequence});
    }
    if (!cfg_.model_tag.empty()) {
        j["model"] = cfg_.model_tag;
    }
    return j.dump();
}

// ---------------------------------------------------------------------------
// Non-streaming generate
// ---------------------------------------------------------------------------
GenResult LlamaCppProvider::generate(const std::vector<ChatMessage>& messages,
                                     const GenOptions& opts)
{
    GenResult result;
    const auto t0 = std::chrono::steady_clock::now();

    const std::string payload = buildPayload(messages, opts, /*stream=*/false);

    httplib::Client cli(cfg_.host, cfg_.port);
    cli.set_connection_timeout(cfg_.connect_timeout_sec, 0);
    cli.set_read_timeout(cfg_.read_timeout_sec, 0);
    cli.set_write_timeout(15, 0);
    cli.set_keep_alive(false);

    httplib::Headers headers = {{"Content-Type", "application/json"}};
    const auto res = cli.Post("/v1/chat/completions", headers, payload,
                              "application/json");

    const auto t1 = std::chrono::steady_clock::now();
    result.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();

    if (!res) {
        result.error = "HTTP error: " + httplib::to_string(res.error());
        return result;
    }
    if (res->status != 200) {
        std::string body = res->body.substr(0, 240);
        result.error = "HTTP " + std::to_string(res->status) + ": " + body;
        return result;
    }

    try {
        const auto j = json::parse(res->body);
        if (j.contains("choices") && j["choices"].is_array()
            && !j["choices"].empty())
        {
            result.text =
                j["choices"][0]["message"]["content"].get<std::string>();
        }
        if (j.contains("usage") && j["usage"].is_object()) {
            if (j["usage"].contains("prompt_tokens"))
                result.prompt_tokens =
                    j["usage"]["prompt_tokens"].get<int>();
            if (j["usage"].contains("completion_tokens"))
                result.completion_tokens =
                    j["usage"]["completion_tokens"].get<int>();
        }
        result.success = true;
        result.cost_usd = 0.0;   // lokal = gratis
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
    }
    return result;
}

// ---------------------------------------------------------------------------
// Streaming generate
// ---------------------------------------------------------------------------
GenResult LlamaCppProvider::streamGenerate(
    const std::vector<ChatMessage>& messages,
    const GenOptions& opts,
    TokenCallback onToken,
    const std::atomic<bool>* cancel_flag)
{
    GenResult result;
    const auto t0 = std::chrono::steady_clock::now();

    const std::string payload = buildPayload(messages, opts, /*stream=*/true);

    httplib::Client cli(cfg_.host, cfg_.port);
    cli.set_connection_timeout(cfg_.connect_timeout_sec, 0);
    cli.set_read_timeout(cfg_.read_timeout_sec, 0);
    cli.set_write_timeout(15, 0);
    cli.set_keep_alive(false);

    std::string accumulated;
    std::string abort_reason;

    SseParser parser([&](const std::string& tok) {
        accumulated += tok;
        if (onToken) onToken(tok);
    });

    httplib::Request req;
    req.method = "POST";
    req.path   = "/v1/chat/completions";
    req.set_header("Content-Type", "application/json");
    req.set_header("Accept", "text/event-stream");
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
        result.error = "HTTP " + std::to_string(res.status) + ": "
                     + res.body.substr(0, 240);
        result.text  = accumulated;
        return result;
    }

    result.success           = true;
    result.text              = accumulated;
    result.prompt_tokens     = parser.prompt_tokens();
    result.completion_tokens = parser.completion_tokens();
    result.cost_usd          = 0.0;   // lokal = gratis
    return result;
}

CostEstimate LlamaCppProvider::estimateCost(
    const std::vector<ChatMessage>& /*messages*/) const
{
    return {};   // lokal = gratis
}

} // namespace frans::infrastructure
