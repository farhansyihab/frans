// ~/AgenAI/frans/tests/deepseek_provider_smoke_test.cpp
//
// Smoke test Stage 8: DeepSeekProvider.
//
// Dua mode:
//   1. OFFLINE (default) — unit test murni: URL parser, cost calculation,
//      config loader, SSE parser. Tidak butuh API key, tidak butuh network.
//   2. LIVE (opt-in) — kalau env DEEPSEEK_LIVE_TEST=1 DAN api_key ada,
//      jalankan 1 call pendek ke DeepSeek API untuk verifikasi end-to-end.
//      Kalau tidak di-set, di-skip dengan pesan jelas.
//
// Jalankan:
//   ./frans_deepseek_test                                    # offline only
//   DEEPSEEK_LIVE_TEST=1 DEEPSEEK_API_KEY=sk-... ./frans_deepseek_test
//
// Stage 8 patch (revisi):
//   1. max_tokens live test dinaikkan dari 16 → 128. Nilai 16 terlalu kecil;
//      model non-reasoning saja butuh >16 token untuk menjawab dengan aman.
//   2. Test 3 (DeepSeekConfig::loadFromFile) sekarang meng-override HOME
//      ke direktori sementara, supaya tidak menyentuh file
//      ~/.config/frans/secrets.json milik user. Tanpa ini, test "api_key
//      harus kosong" gagal begitu user menyimpan API key permanen.

#include "config/DeepSeekConfig.h"
#include "domain/llm/GenOptions.h"
#include "infrastructure/api_providers/DeepSeekProvider.h"
#include "infrastructure/http/SseParser.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

using frans::config::DeepSeekConfig;
using frans::domain::ChatMessage;
using frans::domain::CostEstimate;
using frans::domain::GenOptions;
using frans::domain::GenResult;
using frans::infrastructure::DeepSeekProvider;
using frans::infrastructure::SseParser;

namespace {

int g_failures = 0;

void check(bool cond, const std::string& label) {
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", label.c_str());
    if (!cond) ++g_failures;
}

void banner(const std::string& s) {
    std::printf("\n=== %s ===\n", s.c_str());
}

bool approx(double a, double b, double eps = 1e-9) {
    const double d = a - b;
    return (d < 0 ? -d : d) < eps;
}

std::string makeTempFile(const std::string& tag, const std::string& content) {
    const std::string p = "/tmp/frans_7e8_" + tag + "_"
                        + std::to_string(::getpid()) + ".json";
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    return p;
}

// RAII helper: sementara mengubah HOME ke path sementara, restore saat
// destructor. Dipakai di Test 3 supaya test tidak melihat file secrets.json
// milik user.
class ScopedHomeOverride {
public:
    ScopedHomeOverride() {
        const char* h = std::getenv("HOME");
        saved_ = h ? h : "";
        tmp_home_ = "/tmp/frans_7e8_home_" + std::to_string(::getpid());
        ::setenv("HOME", tmp_home_.c_str(), 1);
    }

    ~ScopedHomeOverride() {
        if (saved_.empty()) {
            ::unsetenv("HOME");
        } else {
            ::setenv("HOME", saved_.c_str(), 1);
        }
    }

    ScopedHomeOverride(const ScopedHomeOverride&)            = delete;
    ScopedHomeOverride& operator=(const ScopedHomeOverride&) = delete;

private:
    std::string saved_;
    std::string tmp_home_;
};

} // namespace

int main() {
    std::printf("=== frans_ai deepseek provider smoke test (Stage 8) ===\n");

    // =====================================================================
    // Test 1 — parseEndpoint
    // =====================================================================
    banner("DeepSeekProvider::parseEndpoint");
    {
        const auto a = DeepSeekProvider::parseEndpoint(
            "https://api.deepseek.com/chat/completions");
        check(a.valid,      "https URL valid");
        check(a.is_https,   "is_https == true");
        check(a.host == "api.deepseek.com", "host benar");
        check(a.port == 443,               "default port 443");
        check(a.path == "/chat/completions", "path benar");

        const auto b = DeepSeekProvider::parseEndpoint(
            "http://localhost:8080/v1/chat/completions");
        check(b.valid,       "http URL valid");
        check(!b.is_https,   "is_https == false");
        check(b.host == "localhost", "host benar");
        check(b.port == 8080, "port eksplisit");
        check(b.path == "/v1/chat/completions", "path benar");

        const auto c = DeepSeekProvider::parseEndpoint(
            "https://api.deepseek.com");
        check(c.valid,       "URL tanpa path valid");
        check(c.path == "/", "path default '/'");

        const auto d = DeepSeekProvider::parseEndpoint("not-a-url");
        check(!d.valid, "URL tanpa skema → invalid");

        const auto e = DeepSeekProvider::parseEndpoint("https://");
        check(!e.valid, "URL tanpa host → invalid");

        const auto f = DeepSeekProvider::parseEndpoint(
            "https://api.deepseek.com:0/x");
        check(!f.valid, "port 0 → invalid");
    }

    // =====================================================================
    // Test 2 — computeActualCost
    // =====================================================================
    banner("DeepSeekProvider::computeActualCost");
    {
        DeepSeekConfig cfg;
        cfg.endpoint = "https://api.deepseek.com/chat/completions";
        cfg.model    = "deepseek-chat";
        cfg.api_key  = "sk-dummy";
        cfg.input_cost_per_mtok_usd  = 0.14;
        cfg.output_cost_per_mtok_usd = 0.28;

        DeepSeekProvider p(std::move(cfg));

        const auto c1 = p.computeActualCost(1'000'000, 0);
        check(approx(c1.input_usd, 0.14),  "1M input tok → $0.14");
        check(approx(c1.output_usd, 0.0),  "0 output tok → $0.00");

        const auto c2 = p.computeActualCost(0, 1'000'000);
        check(approx(c2.input_usd, 0.0),   "0 input tok → $0.00");
        check(approx(c2.output_usd, 0.28), "1M output tok → $0.28");

        const auto c3 = p.computeActualCost(500'000, 250'000);
        check(approx(c3.input_usd,  0.07), "500k input → $0.07");
        check(approx(c3.output_usd, 0.07), "250k output → $0.07");
        check(approx(c3.total_usd(), 0.14), "total = $0.14");
        check(!c3.is_free(),               "not free");
    }

    // =====================================================================
    // Test 3 — DeepSeekConfig loader
    //
    // Test ini sengaja mengisolasi diri dari file
    // ~/.config/frans/secrets.json milik user (via ScopedHomeOverride),
    // supaya verifikasi "api_key kosong" tetap valid baik user sudah
    // menyimpan key permanen atau belum.
    // =====================================================================
    banner("DeepSeekConfig::loadFromFile");
    {
        ScopedHomeOverride home_guard;

        const std::string j = R"({
            "deepseek": {
                "endpoint": "https://api.deepseek.com/chat/completions",
                "model": "deepseek-chat",
                "api_key_env": "FRANS_TEST_DS_KEY_7E8",
                "input_cost_per_mtok_usd": 0.11,
                "output_cost_per_mtok_usd": 0.22,
                "connect_timeout_sec": 7,
                "read_timeout_sec": 42
            }
        })";
        const std::string path = makeTempFile("cfg", j);

        // (a) tanpa env var, tanpa secrets → api_key kosong
        ::unsetenv("FRANS_TEST_DS_KEY_7E8");
        const auto c1 = DeepSeekConfig::loadFromFile(path);
        check(c1.endpoint == "https://api.deepseek.com/chat/completions",
              "endpoint ter-load");
        check(c1.model == "deepseek-chat", "model ter-load");
        check(c1.api_key_env == "FRANS_TEST_DS_KEY_7E8", "api_key_env ter-load");
        check(approx(c1.input_cost_per_mtok_usd, 0.11), "input rate ter-load");
        check(approx(c1.output_cost_per_mtok_usd, 0.22), "output rate ter-load");
        check(c1.connect_timeout_sec == 7, "connect timeout ter-load");
        check(c1.read_timeout_sec == 42, "read timeout ter-load");
        check(!c1.hasApiKey(), "api_key kosong tanpa env");

        // (b) dengan env var
        ::setenv("FRANS_TEST_DS_KEY_7E8", "sk-from-env", 1);
        const auto c2 = DeepSeekConfig::loadFromFile(path);
        check(c2.hasApiKey(), "api_key ada setelah set env");
        check(c2.api_key == "sk-from-env", "api_key dari env benar");
        ::unsetenv("FRANS_TEST_DS_KEY_7E8");

        // (c) file tidak ada → default, no throw. Fallback secrets.json
        // juga tidak ada karena HOME sudah di-override ke path sementara.
        const auto c3 = DeepSeekConfig::loadFromFile("/tmp/nonexistent_xyz_7e8.json");
        check(!c3.hasApiKey(), "file tidak ada → api_key kosong");
        check(c3.endpoint.empty(), "file tidak ada → endpoint kosong");
    }   // ~ScopedHomeOverride() → HOME direstorasi di sini

    // =====================================================================
    // Test 4 — SseParser (shared, offline)
    // =====================================================================
    banner("SseParser — shared");
    {
        std::string acc;
        SseParser parser([&acc](const std::string& tok) { acc += tok; });

        const std::string chunk1 =
            "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n";
        const std::string chunk2 =
            "data: {\"choices\":[{\"delta\":{\"content\":\" world\"}}]}\n\n";
        const std::string chunk3 =
            "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}],"
            "\"usage\":{\"prompt_tokens\":12,\"completion_tokens\":7}}\n\n";
        const std::string chunk4 = "data: [DONE]\n\n";

        parser.feed(chunk1.data(), chunk1.size());
        parser.feed(chunk2.data(), chunk2.size());
        parser.feed(chunk3.data(), chunk3.size());
        parser.feed(chunk4.data(), chunk4.size());

        check(acc == "Hello world",          "content terakumulasi");
        check(parser.done(),                 "done == true setelah [DONE]");
        check(parser.prompt_tokens() == 12,  "prompt_tokens captured");
        check(parser.completion_tokens() == 7, "completion_tokens captured");
        check(parser.finish_reason() == "stop", "finish_reason captured");

        std::string acc2;
        SseParser p2([&acc2](const std::string& tok) { acc2 += tok; });
        const std::string full =
            "data: {\"choices\":[{\"delta\":{\"content\":\"abc\"}}]}\n"
            "data: [DONE]\n";
        for (std::size_t i = 0; i < full.size(); ++i) {
            p2.feed(&full[i], 1);
        }
        check(acc2 == "abc", "byte-by-byte split tetap OK");

        std::string acc3;
        SseParser p3([&acc3](const std::string& tok) { acc3 += tok; });
        const std::string reasoning_chunk =
            "data: {\"choices\":[{\"delta\":"
            "{\"reasoning_content\":\"thinking...\"}}]}\n\n";
        p3.feed(reasoning_chunk.data(), reasoning_chunk.size());
        check(acc3.empty(), "reasoning_content diabaikan");
    }

    // =====================================================================
    // Test 5 — DeepSeekProvider pre-flight checks
    // =====================================================================
    banner("DeepSeekProvider — pre-flight (no api_key)");
    {
        DeepSeekConfig cfg;
        cfg.endpoint = "https://api.deepseek.com/chat/completions";
        cfg.model    = "deepseek-chat";
        cfg.api_key  = "";

        DeepSeekProvider p(std::move(cfg));

        check(!p.isAvailable(), "isAvailable() == false tanpa api_key");

        std::vector<ChatMessage> msgs = {{"user", "hi"}};
        GenOptions opts;

        const auto r = p.generate(msgs, opts);
        check(!r.success, "generate() fail tanpa api_key");
        check(r.error.find("api_key") != std::string::npos,
              "error message menyebut api_key");

        const auto r2 = p.streamGenerate(msgs, opts, nullptr, nullptr);
        check(!r2.success, "streamGenerate() fail tanpa api_key");
        check(r2.error.find("api_key") != std::string::npos,
              "stream error menyebut api_key");
    }

    // =====================================================================
    // Test 6 — invalid endpoint
    // =====================================================================
    banner("DeepSeekProvider — invalid endpoint");
    {
        DeepSeekConfig cfg;
        cfg.endpoint = "not-a-valid-url";
        cfg.model    = "deepseek-chat";
        cfg.api_key  = "sk-dummy";

        DeepSeekProvider p(std::move(cfg));
        check(!p.isAvailable(), "isAvailable() == false untuk endpoint invalid");

        std::vector<ChatMessage> msgs = {{"user", "hi"}};
        GenOptions opts;
        const auto r = p.generate(msgs, opts);
        check(!r.success, "generate() fail untuk endpoint invalid");
        check(r.error.find("endpoint") != std::string::npos,
              "error message menyebut endpoint");
    }

    // =====================================================================
    // Test 7 — estimateCost (offline)
    // =====================================================================
    banner("DeepSeekProvider::estimateCost (offline)");
    {
        DeepSeekConfig cfg;
        cfg.endpoint = "https://api.deepseek.com/chat/completions";
        cfg.model    = "deepseek-chat";
        cfg.api_key  = "sk-dummy";
        cfg.input_cost_per_mtok_usd  = 1.0;
        cfg.output_cost_per_mtok_usd = 2.0;

        DeepSeekProvider p(std::move(cfg));

        std::vector<ChatMessage> msgs = {{"user", "Hello world!"}};
        const auto est = p.estimateCost(msgs);

        std::printf("  estimate: input=$%.9f, output=$%.9f, total=$%.9f\n",
                    est.input_usd, est.output_usd, est.total_usd());

        check(approx(est.input_usd,  0.000004, 1e-10),
              "input estimate = $0.000004");
        check(approx(est.output_usd, 0.001,    1e-10),
              "output estimate = $0.001");
    }

    // =====================================================================
    // Test 8 — LIVE (opt-in)
    // =====================================================================
    banner("LIVE (opt-in) — butuh DEEPSEEK_LIVE_TEST=1 + API key");
    {
        const char* live = std::getenv("DEEPSEEK_LIVE_TEST");
        const bool run_live = (live && std::string(live) == "1");

        if (!run_live) {
            std::printf("  SKIP  (set DEEPSEEK_LIVE_TEST=1 untuk menjalankan)\n");
        } else {
            const auto cfg =
                DeepSeekConfig::loadFromFile("config/providers.json");

            if (!cfg.hasApiKey()) {
                std::printf("  SKIP  (DEEPSEEK_LIVE_TEST=1 tapi api_key kosong)\n");
                std::printf("        set DEEPSEEK_API_KEY di shell ini, "
                            "atau isi ~/.config/frans/secrets.json\n");
            } else {
                std::printf("  api_key OK, menjalankan 1 call live...\n");
                std::printf("  model: %s\n", cfg.model.c_str());

                DeepSeekProvider p(cfg);
                check(p.isAvailable(), "isAvailable() == true");

                std::vector<ChatMessage> msgs = {
                    {"system", "You are a concise assistant. Answer in one sentence."},
                    {"user",   "What is 2+2? Answer with the number only."}
                };
                GenOptions opts;
                opts.max_tokens  = 128;
                opts.temperature = 0.0;

                const auto r = p.generate(msgs, opts);

                std::printf("  text='%s'\n", r.text.c_str());
                std::printf("  prompt=%d completion=%d cost=$%.8f elapsed=%.2fs\n",
                            r.prompt_tokens, r.completion_tokens,
                            r.cost_usd, r.elapsed_sec);
                if (!r.error.empty()) {
                    std::printf("  error: %s\n", r.error.c_str());
                }

                check(r.success, "live generate sukses");
                check(!r.text.empty(), "live text non-empty");
                check(r.prompt_tokens > 0, "prompt_tokens > 0");
                check(r.completion_tokens > 0, "completion_tokens > 0");
                check(r.cost_usd > 0.0, "cost_usd > 0");
                check(r.cost_usd < 0.01, "cost_usd < $0.01 (sanity)");
            }
        }
    }

    std::printf("\n=== %s ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES");
    std::printf("total failures: %d\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
