#pragma once
// Fase C #56 -- GeminiClient (BluePrint v9 Addendum, "Context Pack via Gemini")
//
// Tugas: satu panggilan generateContent yang meminta JSON terstruktur.
// Tidak ada streaming, tidak ada multi-turn, tidak ada tool.
//
// Retry/backoff (default):
//   - 429 (rate limit) & 5xx & network error: retry, backoff 2s lalu 6s (maks 2 retry)
//   - JSON rusak: coba ekstrak dari teks; jika gagal retry maksimal 1x
//   - Timeout / Cancel: tanpa retry
//   - 400 / 401 / 403 / 404: tanpa retry
//   - Blocked / Truncated: tanpa retry (bukan masalah transien)
//
// Semua jeda backoff peka terhadap cancel_flag (dicek per potongan kecil).

#include "infrastructure/http/IHttpTransport.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace frans::application {

struct GeminiRequest {
    std::string model;
    std::string endpoint;
    std::string api_key;
    std::string selector_prompt;     // system instruction
    std::string user_request;        // dibungkus <PERMINTAAN>...</PERMINTAAN>
    std::string repo_context;        // dibungkus <REPO>...</REPO>
    int timeout_sec = 90;
};

struct SelectedFile {
    std::string path;
    std::string priority;   // "must" | "helpful"
    std::string reason;
};

struct GeminiSelection {
    std::vector<SelectedFile> files;
    std::vector<std::string> new_files_expected;
    std::string notes;
};

enum class GeminiErrorKind {
    Ok,
    InvalidKey,        // 400 / 401 / 403
    ModelNotFound,     // 404
    RateLimited,       // 429
    ServerError,       // 5xx
    NetworkError,      // koneksi gagal (status 0)
    Timeout,           // timeout transport
    Cancelled,         // dibatalkan user
    Blocked,           // promptFeedback.blockReason / finishReason SAFETY
    Truncated,         // finishReason != STOP (mis. MAX_TOKENS)
    MalformedJson,     // JSON rusak / tidak sesuai schema
    Unknown
};

const char* toString(GeminiErrorKind kind);

struct GeminiError {
    GeminiErrorKind kind = GeminiErrorKind::Ok;
    std::string message;
    int http_status = 0;
    bool retryable = false;
};

struct GeminiResult {
    bool ok = false;
    GeminiSelection selection;
    GeminiError error;
    int attempts = 0;   // jumlah HTTP request yang benar-benar dikirim
};

// Kebijakan retry. Default sesuai blueprint; boleh diperkecil saat unit test.
struct GeminiRetryPolicy {
    int max_retries = 2;                       // untuk 429 / 5xx / network
    std::vector<int> backoff_ms = {2000, 6000};
    int malformed_json_max_retries = 1;        // JSON rusak: retry sekali
};

class GeminiClient {
public:
    explicit GeminiClient(std::shared_ptr<infrastructure::http::IHttpTransport> transport);

    // Panggil Gemini API untuk memilih file konteks.
    // cancel boleh nullptr. Dicek sebelum request, selama backoff, dan diteruskan
    // ke transport agar request yang sedang berjalan bisa diputus.
    GeminiResult select(const GeminiRequest& req,
                        std::atomic<bool>* cancel = nullptr);

    // Untuk test / tuning. Tidak mengubah perilaku default bila tidak dipanggil.
    void setRetryPolicy(const GeminiRetryPolicy& policy);
    const GeminiRetryPolicy& retryPolicy() const { return policy_; }

private:
    std::shared_ptr<infrastructure::http::IHttpTransport> transport_;
    GeminiRetryPolicy policy_;

    std::string buildRequestBody(const GeminiRequest& req);
    GeminiResult parseResponse(const std::string& body, int http_status);
    GeminiResult extractSelection(const nlohmann::json& selection_json);
    GeminiError mapHttpError(int status, const std::string& body);
};

} // namespace frans::application