// ~/AgenAI/frans/tests/gemini_provider_smoke_test.cpp
//
// Stage 12 — Smoke test untuk GeminiProvider.
//
// CATATAN MODEL: gemini-1.5-flash sudah di-retire Google (24 Sep 2025).
// Default memakai ALIAS "gemini-flash-latest" — Google auto-route ke
// model Flash terbaru (gemini-2.5-flash / 3-flash, dst).
//
// CATATAN THINKING MODEL: gemini-2.5-flash / 3-flash adalah thinking
// models — mereka menghabiskan output token untuk "berpikir" internal
// dulu sebelum mengeluarkan jawaban. max_tokens terlalu kecil (mis. 32)
// menyebabkan finishReason=MAX_TOKENS dengan completion_tokens=0.
// Test ini memakai 512 supaya ada ruang untuk thinking + output.
//
// Cakupan:
//   1. parseEndpoint() — variasi URL valid / tidak valid
//   2. buildEndpoint() — generateContent vs streamGenerateContent
//   3. Provider dengan config kosong → isAvailable() == false
//   4. Live call — config dibaca dari providers.json, fallback ke
//      ~/.config/frans/secrets.json.
//
// Jalankan dari root repo (~/AgenAI/frans) supaya path relatif
// "config/providers.json" resolve.
#include "config/GeminiConfig.h"
#include "infrastructure/api_providers/GeminiProvider.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace frans::config;
using namespace frans::infrastructure;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL: %s at line %d\n", #cond, __LINE__); \
        return 1; \
    } \
} while (0)

namespace {

constexpr const char* kFallbackModel = "gemini-flash-latest";

std::string findProvidersJson() {
    namespace fs = std::filesystem;
    const char* candidates[] = {
        "config/providers.json",
        "../config/providers.json",
        "../../config/providers.json",
    };
    for (const char* c : candidates) {
        if (fs::exists(c)) return c;
    }
    return {};
}

} // namespace

int main() {
    std::printf("[gemini-smoke] starting...\n");

    // ---- 1. parseEndpoint: URL valid HTTPS ----
    {
        auto p = GeminiProvider::parseEndpoint(
            "https://generativelanguage.googleapis.com/v1beta/models/"
            "gemini-flash-latest:generateContent");
        CHECK(p.valid);
        CHECK(p.is_https);
        CHECK(p.host == "generativelanguage.googleapis.com");
        CHECK(p.port == 443);
        CHECK(p.path == "/v1beta/models/gemini-flash-latest:generateContent");
    }

    // ---- 2. parseEndpoint: query string preserved ----
    {
        auto p = GeminiProvider::parseEndpoint(
            "https://generativelanguage.googleapis.com/v1beta/models/"
            "gemini-flash-latest:streamGenerateContent?alt=sse");
        CHECK(p.valid);
        CHECK(p.path == "/v1beta/models/gemini-flash-latest:"
                        "streamGenerateContent?alt=sse");
    }

    // ---- 3. parseEndpoint: HTTP ----
    {
        auto p = GeminiProvider::parseEndpoint("http://localhost:1234/foo");
        CHECK(p.valid);
        CHECK(!p.is_https);
        CHECK(p.host == "localhost");
        CHECK(p.port == 1234);
        CHECK(p.path == "/foo");
    }

    // ---- 4. parseEndpoint: invalid ----
    {
        auto p = GeminiProvider::parseEndpoint("not-a-url");
        CHECK(!p.valid);
    }
    {
        auto p = GeminiProvider::parseEndpoint("https:///foo");
        CHECK(!p.valid);
    }

    // ---- 5. buildEndpoint (non-stream) ----
    {
        auto url = GeminiProvider::buildEndpoint(
            "https://generativelanguage.googleapis.com/v1beta",
            "gemini-flash-latest", /*stream=*/false);
        CHECK(url == "https://generativelanguage.googleapis.com/v1beta/"
                     "models/gemini-flash-latest:generateContent");
    }

    // ---- 6. buildEndpoint (stream) ----
    {
        auto url = GeminiProvider::buildEndpoint(
            "https://generativelanguage.googleapis.com/v1beta",
            "gemini-flash-latest", /*stream=*/true);
        CHECK(url == "https://generativelanguage.googleapis.com/v1beta/"
                     "models/gemini-flash-latest:streamGenerateContent?alt=sse");
    }

    // ---- 7. buildEndpoint: trailing slash di base ----
    {
        auto url = GeminiProvider::buildEndpoint(
            "https://example.com/v1beta/", "m", false);
        CHECK(url == "https://example.com/v1beta/models/m:generateContent");
    }

    // ---- 8. Provider dengan config kosong → isAvailable() == false ----
    {
        GeminiConfig empty_cfg;   // enabled == false by default
        GeminiProvider p(empty_cfg);
        CHECK(!p.isAvailable());
        CHECK(p.name() == "Gemini");

        std::vector<frans::domain::ChatMessage> msgs;
        msgs.push_back({"user", "hi"});
        frans::domain::GenOptions opts;
        auto r = p.generate(msgs, opts);
        CHECK(!r.success);
        CHECK(!r.error.empty());
    }

    // =====================================================================
    // 9. Live call (opsional) — config resolution berlapis.
    // =====================================================================
    GeminiConfig cfg;

    const std::string providers_path = findProvidersJson();
    if (providers_path.empty()) {
        std::printf("[gemini-smoke] config/providers.json tidak ditemukan "
                    "dari cwd — jalankan dari root repo.\n");
    } else {
        std::printf("[gemini-smoke] memuat config dari '%s'...\n",
                    providers_path.c_str());
        cfg = GeminiConfig::loadFromFile(providers_path);
        std::printf("[gemini-smoke] endpoint='%s' model='%s'\n",
                    cfg.endpoint.c_str(), cfg.model.c_str());
    }

    // 9b. Env var override (kalau diset, prioritaskan ini).
    const char* env_key = std::getenv("GEMINI_API_KEY");
    if (env_key && *env_key) {
        std::printf("[gemini-smoke] GEMINI_API_KEY dari env — override.\n");
        cfg.api_key = env_key;
        if (cfg.api_key_env.empty()) cfg.api_key_env = "GEMINI_API_KEY";
        if (cfg.endpoint.empty())
            cfg.endpoint =
                "https://generativelanguage.googleapis.com/v1beta";
        if (cfg.model.empty()) cfg.model = kFallbackModel;
    }

    // 9c. Fallback terakhir: isi minimal supaya kita bisa cek API key
    //     langsung dari secrets.json tanpa bergantung pada providers.json.
    if (cfg.endpoint.empty()) {
        cfg.endpoint = "https://generativelanguage.googleapis.com/v1beta";
    }
    if (cfg.model.empty()) {
        cfg.model = kFallbackModel;
    }

    if (!cfg.hasApiKey()) {
        std::printf("[gemini-smoke] api_key KOSONG — skip live call.\n");
        std::printf("  Sumber yang dicek:\n");
        std::printf("    - env GEMINI_API_KEY\n");
        std::printf("    - providers.json blok 'gemini'.api_key_env\n");
        std::printf("    - %s  (key \"gemini_api_key\")\n",
                    GeminiConfig::defaultSecretsPath().c_str());
        std::printf("[gemini-smoke] all offline checks passed\n");
        return 0;
    }

    std::printf("[gemini-smoke] api_key ADA (%zu chars) — live call "
                "ke model '%s'...\n",
                cfg.api_key.size(), cfg.model.c_str());

    GeminiProvider provider(cfg);
    CHECK(provider.isAvailable());

    std::vector<frans::domain::ChatMessage> msgs;
    msgs.push_back({"user", "Reply with just the word: ok"});

    frans::domain::GenOptions opts;
    // Thinking model: butuh ruang untuk internal reasoning + output.
    // 512 cukup untuk task trivial (thinking ~50-200 tok).
    // Kalau naik ke 1024 masih EMPTY, cek apakah model butuh lebih besar
    // (mis. gemini-2.5-pro / gemini-3-pro pakai thinking lebih banyak).
    opts.max_tokens = 512;

    auto r = provider.generate(msgs, opts);
    if (!r.success) {
        std::fprintf(stderr,
            "[gemini-smoke] live generate() gagal: %s\n",
            r.error.c_str());
        // Jangan fail — network / quota bisa jadi penyebab.
    } else {
        std::printf("[gemini-smoke] live OK: text='%s', "
                    "tokens=%d/%d, %.2fs\n",
                    r.text.c_str(), r.prompt_tokens,
                    r.completion_tokens, r.elapsed_sec);
    }

    std::printf("[gemini-smoke] all checks passed\n");
    return 0;
}