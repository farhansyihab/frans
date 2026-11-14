// Fase C #56 -- GeminiClient implementation
#include "GeminiClient.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

namespace frans::application {

using json = nlohmann::json;

namespace {

bool isCancelled(std::atomic<bool>* cancel) {
    return cancel && cancel->load(std::memory_order_relaxed);
}

// Sleep yang bisa diputus. Return false bila dibatalkan.
bool cancellableSleep(int total_ms, std::atomic<bool>* cancel) {
    if (isCancelled(cancel)) return false;
    constexpr int kSliceMs = 20;
    int slept = 0;
    while (slept < total_ms) {
        int step = std::min(kSliceMs, total_ms - slept);
        std::this_thread::sleep_for(std::chrono::milliseconds(step));
        slept += step;
        if (isCancelled(cancel)) return false;
    }
    return !isCancelled(cancel);
}

GeminiError makeCancelled(const std::string& msg) {
    GeminiError e;
    e.kind = GeminiErrorKind::Cancelled;
    e.message = msg;
    e.retryable = false;
    return e;
}

// Ambil pesan error dari body Google API bila ada, untuk pesan yang actionable.
std::string apiErrorMessage(const std::string& body) {
    try {
        json j = json::parse(body);
        if (j.is_object() && j.contains("error") && j["error"].is_object()) {
            const auto& e = j["error"];
            std::string msg;
            if (e.contains("status") && e["status"].is_string())
                msg += e["status"].get<std::string>();
            if (e.contains("message") && e["message"].is_string()) {
                if (!msg.empty()) msg += ": ";
                msg += e["message"].get<std::string>();
            }
            if (msg.size() > 300) msg = msg.substr(0, 300) + "...";
            return msg;
        }
    } catch (...) {
        // body bukan JSON, abaikan
    }
    return std::string();
}

} // namespace

const char* toString(GeminiErrorKind kind) {
    switch (kind) {
        case GeminiErrorKind::Ok:            return "Ok";
        case GeminiErrorKind::InvalidKey:    return "InvalidKey";
        case GeminiErrorKind::ModelNotFound: return "ModelNotFound";
        case GeminiErrorKind::RateLimited:   return "RateLimited";
        case GeminiErrorKind::ServerError:   return "ServerError";
        case GeminiErrorKind::NetworkError:  return "NetworkError";
        case GeminiErrorKind::Timeout:       return "Timeout";
        case GeminiErrorKind::Cancelled:     return "Cancelled";
        case GeminiErrorKind::Blocked:       return "Blocked";
        case GeminiErrorKind::Truncated:     return "Truncated";
        case GeminiErrorKind::MalformedJson: return "MalformedJson";
        case GeminiErrorKind::Unknown:       return "Unknown";
    }
    return "?";
}

GeminiClient::GeminiClient(std::shared_ptr<infrastructure::http::IHttpTransport> transport)
    : transport_(std::move(transport)) {}

void GeminiClient::setRetryPolicy(const GeminiRetryPolicy& policy) {
    policy_ = policy;
    if (policy_.max_retries < 0) policy_.max_retries = 0;
    if (policy_.malformed_json_max_retries < 0) policy_.malformed_json_max_retries = 0;
    if (policy_.backoff_ms.empty()) policy_.backoff_ms.push_back(0);
}

std::string GeminiClient::buildRequestBody(const GeminiRequest& req) {
    json j;

    j["systemInstruction"] = {
        {"parts", {{{"text", req.selector_prompt}}}}
    };

    const std::string user_text = "<PERMINTAAN>\n" + req.user_request
                                + "\n</PERMINTAAN>\n<REPO>\n"
                                + req.repo_context + "\n</REPO>";
    j["contents"] = {
        {
            {"role", "user"},
            {"parts", {{{"text", user_text}}}}
        }
    };

    j["generationConfig"] = {
        {"responseMimeType", "application/json"},
        {"temperature", 0.1},
        {"responseSchema", {
            {"type", "OBJECT"},
            {"properties", {
                {"files", {
                    {"type", "ARRAY"},
                    {"items", {
                        {"type", "OBJECT"},
                        {"properties", {
                            {"path", {{"type", "STRING"}}},
                            {"priority", {
                                {"type", "STRING"},
                                {"enum", {"must", "helpful"}}
                            }},
                            {"reason", {{"type", "STRING"}}}
                        }},
                        {"required", {"path", "priority", "reason"}}
                    }}
                }},
                {"new_files_expected", {
                    {"type", "ARRAY"},
                    {"items", {{"type", "STRING"}}}
                }},
                {"notes", {{"type", "STRING"}}}
            }},
            {"required", {"files"}}
        }}
    };

    return j.dump();
}

GeminiResult GeminiClient::select(const GeminiRequest& req, std::atomic<bool>* cancel) {
    GeminiResult result;

    if (!transport_) {
        result.error.kind = GeminiErrorKind::Unknown;
        result.error.message = "HTTP transport belum diinisialisasi";
        return result;
    }

    infrastructure::http::HttpRequest http_req;
    http_req.url = req.endpoint + "/models/" + req.model + ":generateContent";
    http_req.headers_secret = {"x-goog-api-key: " + req.api_key};
    http_req.headers_plain = {"Content-Type: application/json"};
    http_req.body = buildRequestBody(req);
    http_req.timeout_sec = req.timeout_sec;

    int sent = 0;
    int http_retries = 0;
    int json_retries = 0;

    for (;;) {
        if (isCancelled(cancel)) {
            result.ok = false;
            result.error = makeCancelled("Dibatalkan oleh pengguna");
            result.attempts = sent;
            return result;
        }

        auto response = transport_->post(http_req, cancel);
        ++sent;
        result.attempts = sent;

        if (response.cancelled) {
            result.ok = false;
            result.error = makeCancelled("Dibatalkan oleh pengguna");
            return result;
        }

        if (response.timed_out) {
            result.ok = false;
            result.error.kind = GeminiErrorKind::Timeout;
            result.error.message = "Timeout menunggu respons Gemini ("
                                 + std::to_string(req.timeout_sec) + "s)";
            result.error.retryable = false;
            return result;   // tanpa retry
        }

        GeminiError err;
        bool malformed = false;

        if (response.status == 200) {
            GeminiResult parsed = parseResponse(response.body, response.status);
            parsed.attempts = sent;
            if (parsed.ok) return parsed;
            err = parsed.error;
            malformed = (err.kind == GeminiErrorKind::MalformedJson);
        } else if (response.status == 0) {
            err.kind = GeminiErrorKind::NetworkError;
            err.message = "Error jaringan: "
                        + (response.error.empty() ? std::string("koneksi gagal")
                                                  : response.error);
            err.retryable = true;
        } else {
            err = mapHttpError(response.status, response.body);
        }

        result.ok = false;
        result.error = err;
        result.attempts = sent;

        const bool allow_retry =
            malformed ? (json_retries < policy_.malformed_json_max_retries)
                      : (err.retryable && http_retries < policy_.max_retries);

        if (!allow_retry) return result;

        const int total_retries = http_retries + json_retries;
        const int idx = std::min(total_retries,
                                 static_cast<int>(policy_.backoff_ms.size()) - 1);
        const int delay_ms = policy_.backoff_ms[static_cast<size_t>(idx)];

        if (!cancellableSleep(delay_ms, cancel)) {
            result.error = makeCancelled("Dibatalkan selama backoff");
            result.attempts = sent;
            return result;
        }

        if (malformed) ++json_retries;
        else ++http_retries;
    }
}

GeminiResult GeminiClient::parseResponse(const std::string& body, int http_status) {
    GeminiResult result;
    result.ok = false;
    result.error.http_status = http_status;

    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error& e) {
        result.error.kind = GeminiErrorKind::MalformedJson;
        result.error.message = std::string("Envelope respons bukan JSON valid: ") + e.what();
        result.error.retryable = true;
        return result;
    }

    if (j.contains("promptFeedback") && j["promptFeedback"].is_object()
        && j["promptFeedback"].contains("blockReason")) {
        result.error.kind = GeminiErrorKind::Blocked;
        std::string reason = "UNKNOWN";
        const auto& br = j["promptFeedback"]["blockReason"];
        if (br.is_string()) reason = br.get<std::string>();
        result.error.message = "Permintaan diblokir Gemini: " + reason
                             + ". Perkecil konteks atau ubah kalimat permintaan.";
        result.error.retryable = false;
        return result;
    }

    if (!j.contains("candidates") || !j["candidates"].is_array()
        || j["candidates"].empty()) {
        result.error.kind = GeminiErrorKind::Unknown;
        result.error.message = "Respons Gemini tidak memiliki candidates";
        result.error.retryable = false;
        return result;
    }

    const auto& candidate = j["candidates"][0];

    if (candidate.contains("finishReason") && candidate["finishReason"].is_string()) {
        const std::string finish = candidate["finishReason"].get<std::string>();
        if (finish == "SAFETY" || finish == "PROHIBITED_CONTENT"
            || finish == "BLOCKLIST" || finish == "SPII") {
            result.error.kind = GeminiErrorKind::Blocked;
            result.error.message = "Respons diblokir Gemini (finishReason=" + finish + ")";
            result.error.retryable = false;
            return result;
        }
        if (finish != "STOP") {
            result.error.kind = GeminiErrorKind::Truncated;
            result.error.message = "Respons terpotong (finishReason=" + finish
                                 + "). Perkecil daftar file atau naikkan batas token.";
            result.error.retryable = false;
            return result;
        }
    }

    if (!candidate.contains("content") || !candidate["content"].is_object()
        || !candidate["content"].contains("parts")
        || !candidate["content"]["parts"].is_array()
        || candidate["content"]["parts"].empty()) {
        result.error.kind = GeminiErrorKind::Unknown;
        result.error.message = "Respons Gemini tidak memiliki content.parts";
        result.error.retryable = false;
        return result;
    }

    // Gabungkan seluruh parts bertipe text (model kadang memecah output).
    std::string text;
    for (const auto& part : candidate["content"]["parts"]) {
        if (part.is_object() && part.contains("text") && part["text"].is_string())
            text += part["text"].get<std::string>();
    }

    if (text.empty()) {
        result.error.kind = GeminiErrorKind::MalformedJson;
        result.error.message = "Respons Gemini kosong (tidak ada text)";
        result.error.retryable = true;
        return result;
    }

    try {
        return extractSelection(json::parse(text));
    } catch (const json::parse_error&) {
        // Fallback: model kadang membungkus JSON dengan prose / code fence.
        const auto start = text.find('{');
        const auto end = text.rfind('}');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            try {
                return extractSelection(json::parse(text.substr(start, end - start + 1)));
            } catch (...) {
                // jatuh ke error di bawah
            }
        }
        result.error.kind = GeminiErrorKind::MalformedJson;
        result.error.message = "JSON pemilihan file rusak dan tidak bisa diekstrak";
        result.error.retryable = true;
        return result;
    }
}

GeminiResult GeminiClient::extractSelection(const json& selection_json) {
    GeminiResult result;

    if (!selection_json.is_object() || !selection_json.contains("files")
        || !selection_json["files"].is_array()) {
        result.error.kind = GeminiErrorKind::MalformedJson;
        result.error.message = "JSON tidak sesuai schema: field 'files' (array) tidak ada";
        result.error.retryable = true;
        return result;
    }

    result.ok = true;

    for (const auto& f : selection_json["files"]) {
        if (!f.is_object()) continue;
        SelectedFile sf;
        sf.path = f.value("path", "");
        sf.priority = f.value("priority", "helpful");
        sf.reason = f.value("reason", "");
        if (sf.priority != "must" && sf.priority != "helpful") sf.priority = "helpful";
        if (!sf.path.empty()) result.selection.files.push_back(sf);
    }

    if (selection_json.contains("new_files_expected")
        && selection_json["new_files_expected"].is_array()) {
        for (const auto& nf : selection_json["new_files_expected"]) {
            if (nf.is_string()) {
                std::string p = nf.get<std::string>();
                if (!p.empty()) result.selection.new_files_expected.push_back(p);
            }
        }
    }

    if (selection_json.contains("notes") && selection_json["notes"].is_string())
        result.selection.notes = selection_json["notes"].get<std::string>();

    return result;
}

GeminiError GeminiClient::mapHttpError(int status, const std::string& body) {
    GeminiError err;
    err.http_status = status;

    if (status == 400 || status == 401 || status == 403) {
        err.kind = GeminiErrorKind::InvalidKey;
        err.message = "API key tidak valid / tidak berizin (HTTP "
                    + std::to_string(status) + "). Periksa GEMINI_API_KEY.";
        err.retryable = false;
    } else if (status == 404) {
        err.kind = GeminiErrorKind::ModelNotFound;
        err.message = "Model tidak ditemukan (HTTP 404). Ganti gemini_model di config "
                      "atau pakai tombol 'Tes Koneksi'.";
        err.retryable = false;
    } else if (status == 429) {
        err.kind = GeminiErrorKind::RateLimited;
        err.message = "Kuota API terlampaui (HTTP 429). Tunggu beberapa saat, "
                      "perkecil konteks, atau ganti model.";
        err.retryable = true;
    } else if (status >= 500) {
        err.kind = GeminiErrorKind::ServerError;
        err.message = "Server Gemini bermasalah (HTTP " + std::to_string(status) + ")";
        err.retryable = true;
    } else {
        err.kind = GeminiErrorKind::Unknown;
        err.message = "HTTP error " + std::to_string(status);
        err.retryable = false;
    }

    const std::string detail = apiErrorMessage(body);
    if (!detail.empty()) err.message += " [" + detail + "]";

    return err;
}

} // namespace frans::application