// Fase C #56 -- unit test GeminiClient dengan transport palsu (offline).
//
// Cakupan:
//   A. build request JSON (schema, tag, header, url, timeout, kebocoran key)
//   B. parse respons sukses (+ fallback code fence / prose)
//   C. JSON rusak -> retry sekali
//   D. pemetaan error 400/401/403/404/429/5xx/network/timeout
//   E. blocked & truncated
//   F. retry/backoff + cancel (sebelum request, saat backoff, saat transport)
//
// Jalankan: ./build/frans_gemini_tests [substring-filter] | --list

#include "application/ai/GeminiClient.h"
#include "FakeHttpTransport.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using json = nlohmann::json;
using namespace frans::application;
using frans::test::FakeHttpTransport;
using frans::test::FakeStep;

// ---------------------------------------------------------------- mini harness

namespace {

int g_checks_failed = 0;
std::string g_current;

template <class T>
std::string toStr(const T& v) {
    if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(v));
    } else {
        std::ostringstream os;
        os << v;
        return os.str();
    }
}

void report(bool ok, const std::string& expr, int line, const std::string& extra = "") {
    if (ok) return;
    ++g_checks_failed;
    std::cout << "    FAIL line " << line << ": " << expr;
    if (!extra.empty()) std::cout << "  -> " << extra;
    std::cout << "\n";
}

void expectTrue(bool cond, const char* expr, int line) {
    report(cond, expr, line);
}

template <class A, class B>
void expectEq(const A& a, const B& b, const char* ea, const char* eb, int line) {
    const bool ok = (a == b);
    report(ok, std::string(ea) + " == " + eb, line,
           ok ? "" : ("got [" + toStr(a) + "] expected [" + toStr(b) + "]"));
}

#define EXPECT_TRUE(x)       expectTrue(static_cast<bool>(x), #x, __LINE__)
#define EXPECT_EQ(a, b)      expectEq((a), (b), #a, #b, __LINE__)

// ---------------------------------------------------------------- fixtures

GeminiRequest baseReq() {
    GeminiRequest r;
    r.model = "gemini-2.0-flash";
    r.endpoint = "https://generativelanguage.googleapis.com/v1beta";
    r.api_key = "SECRET-TEST-KEY-abc123";
    r.selector_prompt = "Anda pemilih file konteks. Balas JSON sesuai schema.";
    r.user_request = "tambah fitur login guru";
    r.repo_context = "## Struktur Repo\n- src/main.cpp\n- src/ui/LoginForm.cpp\n";
    r.timeout_sec = 17;
    return r;
}

GeminiRetryPolicy fastPolicy() {
    GeminiRetryPolicy p;
    p.max_retries = 2;
    p.backoff_ms = {5, 10};
    p.malformed_json_max_retries = 1;
    return p;
}

json selectionJson() {
    return json{
        {"files", json::array({
            json{{"path", "src/main.cpp"}, {"priority", "must"}, {"reason", "entry point"}},
            json{{"path", "src/ui/LoginForm.cpp"}, {"priority", "helpful"}, {"reason", "pola form"}}
        })},
        {"new_files_expected", json::array({"src/auth/LoginService.cpp"})},
        {"notes", "pakai pola form yang sudah ada"}
    };
}

// Bungkus selection menjadi envelope generateContent yang normal.
std::string envelope(const std::string& text, const std::string& finish = "STOP") {
    json j;
    j["candidates"] = json::array({
        json{
            {"content", {{"role", "model"}, {"parts", json::array({json{{"text", text}}})}}},
            {"finishReason", finish}
        }
    });
    return j.dump();
}

std::string okBody() { return envelope(selectionJson().dump()); }

std::string fence() { return std::string(3, '`'); }   // hindari literal backtick berurutan

FakeStep httpOk(const std::string& body) {
    FakeStep s; s.status = 200; s.body = body; return s;
}

FakeStep httpErr(int status, const std::string& msg = "boom") {
    FakeStep s;
    s.status = status;
    s.body = json{{"error", {{"code", status}, {"status", "ERR"}, {"message", msg}}}}.dump();
    return s;
}

struct Fixture {
    std::shared_ptr<FakeHttpTransport> fake = std::make_shared<FakeHttpTransport>();
    GeminiClient client{fake};
    Fixture() { client.setRetryPolicy(fastPolicy()); }
};

long long msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - t0).count();
}

// ---------------------------------------------------------------- A. request

void test_request_json_structure() {
    Fixture f;
    f.fake->script = {httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(f.fake->requests.size(), static_cast<size_t>(1));

    const auto& hr = f.fake->requests[0];
    EXPECT_EQ(hr.url,
              std::string("https://generativelanguage.googleapis.com/v1beta/models/"
                          "gemini-2.0-flash:generateContent"));
    EXPECT_EQ(hr.timeout_sec, 17);

    bool has_key_header = false, has_ctype = false;
    for (const auto& h : hr.headers_secret)
        if (h == "x-goog-api-key: SECRET-TEST-KEY-abc123") has_key_header = true;
    for (const auto& h : hr.headers_plain)
        if (h == "Content-Type: application/json") has_ctype = true;
    EXPECT_TRUE(has_key_header);
    EXPECT_TRUE(has_ctype);

    // API key tidak boleh muncul di body maupun header non-secret.
    EXPECT_TRUE(hr.body.find("SECRET-TEST-KEY") == std::string::npos);
    for (const auto& h : hr.headers_plain)
        EXPECT_TRUE(h.find("SECRET-TEST-KEY") == std::string::npos);

    json b = json::parse(hr.body);
    EXPECT_EQ(b["systemInstruction"]["parts"][0]["text"].get<std::string>(),
              baseReq().selector_prompt);
    const std::string ut = b["contents"][0]["parts"][0]["text"].get<std::string>();
    EXPECT_EQ(b["contents"][0]["role"].get<std::string>(), std::string("user"));
    EXPECT_TRUE(ut.find("<PERMINTAAN>") != std::string::npos);
    EXPECT_TRUE(ut.find("</PERMINTAAN>") != std::string::npos);
    EXPECT_TRUE(ut.find("<REPO>") != std::string::npos);
    EXPECT_TRUE(ut.find("</REPO>") != std::string::npos);
    EXPECT_TRUE(ut.find("tambah fitur login guru") != std::string::npos);
    EXPECT_TRUE(ut.find("src/ui/LoginForm.cpp") != std::string::npos);

    const auto& gc = b["generationConfig"];
    EXPECT_EQ(gc["responseMimeType"].get<std::string>(), std::string("application/json"));
    EXPECT_TRUE(gc.contains("responseSchema"));
    const auto& schema = gc["responseSchema"];
    EXPECT_EQ(schema["type"].get<std::string>(), std::string("OBJECT"));
    EXPECT_TRUE(schema.contains("properties"));
    EXPECT_EQ(schema["required"][0].get<std::string>(), std::string("files"));
    EXPECT_EQ(schema["properties"]["files"]["items"]["properties"]["priority"]["enum"][0]
                  .get<std::string>(), std::string("must"));
}

// ---------------------------------------------------------------- B. sukses

void test_success_parses_selection() {
    Fixture f;
    f.fake->script = {httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.attempts, 1);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Ok);
    EXPECT_EQ(res.selection.files.size(), static_cast<size_t>(2));
    EXPECT_EQ(res.selection.files[0].path, std::string("src/main.cpp"));
    EXPECT_EQ(res.selection.files[0].priority, std::string("must"));
    EXPECT_EQ(res.selection.files[0].reason, std::string("entry point"));
    EXPECT_EQ(res.selection.files[1].priority, std::string("helpful"));
    EXPECT_EQ(res.selection.new_files_expected.size(), static_cast<size_t>(1));
    EXPECT_EQ(res.selection.new_files_expected[0], std::string("src/auth/LoginService.cpp"));
    EXPECT_EQ(res.selection.notes, std::string("pakai pola form yang sudah ada"));
}

void test_success_from_code_fence() {
    Fixture f;
    const std::string text = fence() + "json\n" + selectionJson().dump() + "\n" + fence();
    f.fake->script = {httpOk(envelope(text))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.attempts, 1);
    EXPECT_EQ(res.selection.files.size(), static_cast<size_t>(2));
}

void test_success_from_prose_wrapper() {
    Fixture f;
    const std::string text = "Baik, ini hasilnya:\n" + selectionJson().dump()
                           + "\nSemoga membantu.";
    f.fake->script = {httpOk(envelope(text))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.selection.files.size(), static_cast<size_t>(2));
}

void test_success_multi_parts_and_bad_priority() {
    Fixture f;
    json sel = selectionJson();
    sel["files"][0]["priority"] = "urgent";   // di luar enum -> normalisasi helpful
    const std::string half1 = sel.dump().substr(0, 20);
    const std::string half2 = sel.dump().substr(20);
    json j;
    j["candidates"] = json::array({json{
        {"content", {{"parts", json::array({json{{"text", half1}}, json{{"text", half2}}})}}},
        {"finishReason", "STOP"}}});
    f.fake->script = {httpOk(j.dump())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.selection.files.size(), static_cast<size_t>(2));
    EXPECT_EQ(res.selection.files[0].priority, std::string("helpful"));
}

void test_success_empty_files_is_valid() {
    Fixture f;
    json sel = json{{"files", json::array()}};
    f.fake->script = {httpOk(envelope(sel.dump()))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.selection.files.size(), static_cast<size_t>(0));
}

// ---------------------------------------------------------------- C. JSON rusak

void test_malformed_json_retries_once_then_fails() {
    Fixture f;
    f.fake->script = {httpOk(envelope("ini bukan json sama sekali")),
                      httpOk(envelope("masih bukan json"))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::MalformedJson);
    EXPECT_EQ(res.attempts, 2);              // 1 awal + 1 retry, bukan 3
    EXPECT_EQ(f.fake->post_calls, 2);
}

void test_malformed_json_then_success() {
    Fixture f;
    f.fake->script = {httpOk(envelope("{oops")), httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.attempts, 2);
    EXPECT_EQ(res.selection.files.size(), static_cast<size_t>(2));
}

void test_missing_files_field_is_malformed() {
    Fixture f;
    json sel = json{{"notes", "lupa files"}};
    f.fake->script = {httpOk(envelope(sel.dump())), httpOk(envelope(sel.dump()))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::MalformedJson);
    EXPECT_EQ(res.attempts, 2);
}

void test_broken_envelope_is_malformed() {
    Fixture f;
    f.fake->script = {httpOk("<html>gateway</html>"), httpOk(envelope("<html>gateway</html>"))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::MalformedJson);
    EXPECT_EQ(res.attempts, 2);
}

void test_empty_text_is_malformed() {
    Fixture f;
    f.fake->script = {httpOk(envelope("")), httpOk(envelope(""))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::MalformedJson);
}

// ---------------------------------------------------------------- D. error HTTP

void test_http_400_no_retry() {
    Fixture f;
    f.fake->script = {httpErr(400, "API_KEY_INVALID")};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::InvalidKey);
    EXPECT_EQ(res.error.http_status, 400);
    EXPECT_TRUE(!res.error.retryable);
    EXPECT_EQ(res.attempts, 1);
    EXPECT_TRUE(res.error.message.find("API_KEY_INVALID") != std::string::npos);
}

void test_http_401_and_403_no_retry() {
    for (int status : {401, 403}) {
        Fixture f;
        f.fake->script = {httpErr(status, "PERMISSION_DENIED")};
        auto res = f.client.select(baseReq());
        EXPECT_TRUE(!res.ok);
        EXPECT_EQ(res.error.kind, GeminiErrorKind::InvalidKey);
        EXPECT_EQ(res.error.http_status, status);
        EXPECT_EQ(res.attempts, 1);
    }
}

void test_http_404_model_not_found() {
    Fixture f;
    f.fake->script = {httpErr(404, "model not found")};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::ModelNotFound);
    EXPECT_EQ(res.attempts, 1);
    EXPECT_TRUE(res.error.message.find("gemini_model") != std::string::npos);
}

void test_http_429_then_success() {
    Fixture f;
    f.fake->script = {httpErr(429, "RESOURCE_EXHAUSTED"), httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.attempts, 2);
    EXPECT_EQ(res.selection.files.size(), static_cast<size_t>(2));
}

void test_http_429_persistent_exhausts_retries() {
    Fixture f;
    f.fake->script = {httpErr(429), httpErr(429), httpErr(429), httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::RateLimited);
    EXPECT_EQ(res.attempts, 3);              // 1 + max_retries(2)
    EXPECT_EQ(f.fake->post_calls, 3);
}

void test_http_503_then_success() {
    Fixture f;
    f.fake->script = {httpErr(503, "UNAVAILABLE"), httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.attempts, 2);
}

void test_http_500_persistent() {
    Fixture f;
    f.fake->script = {httpErr(500), httpErr(500), httpErr(500)};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::ServerError);
    EXPECT_EQ(res.attempts, 3);
}

void test_network_error_then_success() {
    Fixture f;
    FakeStep net; net.status = 0; net.error = "Could not resolve host";
    f.fake->script = {net, httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.attempts, 2);
}

void test_network_error_persistent() {
    Fixture f;
    FakeStep net; net.status = 0; net.error = "Connection refused";
    f.fake->script = {net};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::NetworkError);
    EXPECT_EQ(res.attempts, 3);
}

void test_timeout_no_retry() {
    Fixture f;
    FakeStep to; to.status = 0; to.timed_out = true;
    f.fake->script = {to, httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Timeout);
    EXPECT_EQ(res.attempts, 1);
}

void test_unknown_http_status_no_retry() {
    Fixture f;
    f.fake->script = {httpErr(418, "teapot")};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Unknown);
    EXPECT_EQ(res.attempts, 1);
}

// ---------------------------------------------------------------- E. blocked/truncated

void test_prompt_feedback_blocked() {
    Fixture f;
    json j = json{{"promptFeedback", {{"blockReason", "SAFETY"}}}};
    f.fake->script = {httpOk(j.dump()), httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Blocked);
    EXPECT_EQ(res.attempts, 1);              // bukan transien -> tanpa retry
    EXPECT_TRUE(res.error.message.find("SAFETY") != std::string::npos);
}

void test_finish_reason_safety_blocked() {
    Fixture f;
    f.fake->script = {httpOk(envelope(selectionJson().dump(), "SAFETY"))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Blocked);
    EXPECT_EQ(res.attempts, 1);
}

void test_finish_reason_max_tokens_truncated() {
    Fixture f;
    // JSON sengaja terpotong seperti kasus nyata MAX_TOKENS.
    f.fake->script = {httpOk(envelope("{\"files\":[{\"path\":\"src/main.c", "MAX_TOKENS"))};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Truncated);
    EXPECT_EQ(res.attempts, 1);
    EXPECT_TRUE(res.error.message.find("MAX_TOKENS") != std::string::npos);
}

void test_no_candidates_is_unknown() {
    Fixture f;
    f.fake->script = {httpOk(json{{"candidates", json::array()}}.dump())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Unknown);
    EXPECT_EQ(res.attempts, 1);
}

// ---------------------------------------------------------------- F. cancel & backoff

void test_cancel_before_request() {
    Fixture f;
    f.fake->script = {httpOk(okBody())};
    std::atomic<bool> cancel{true};
    auto res = f.client.select(baseReq(), &cancel);
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Cancelled);
    EXPECT_EQ(res.attempts, 0);
    EXPECT_EQ(f.fake->post_calls, 0);        // tidak ada trafik sama sekali
}

void test_cancel_during_backoff_uses_default_2s() {
    // Pakai policy DEFAULT (2s, 6s) untuk membuktikan backoff bisa diputus cepat.
    auto fake = std::make_shared<FakeHttpTransport>();
    GeminiClient client(fake);
    fake->script = {httpErr(429), httpOk(okBody())};

    std::atomic<bool> cancel{false};
    std::thread canceller([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        cancel.store(true);
    });

    const auto t0 = std::chrono::steady_clock::now();
    auto res = client.select(baseReq(), &cancel);
    const long long elapsed = msSince(t0);
    canceller.join();

    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Cancelled);
    EXPECT_EQ(res.attempts, 1);
    EXPECT_EQ(fake->post_calls, 1);          // retry tidak jadi dikirim
    EXPECT_TRUE(elapsed < 1200);             // bukan menunggu 2000 ms penuh
    std::cout << "    info: cancel backoff responsif dalam " << elapsed << " ms\n";
}

void test_cancel_during_transport() {
    Fixture f;
    FakeStep slow = httpOk(okBody());
    slow.delay_ms = 600;
    f.fake->script = {slow};

    std::atomic<bool> cancel{false};
    std::thread canceller([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        cancel.store(true);
    });

    const auto t0 = std::chrono::steady_clock::now();
    auto res = f.client.select(baseReq(), &cancel);
    const long long elapsed = msSince(t0);
    canceller.join();

    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Cancelled);
    EXPECT_EQ(res.attempts, 1);
    EXPECT_TRUE(f.fake->cancel_seen > 0);
    EXPECT_TRUE(elapsed < 500);
}

void test_no_cancel_completes_normally() {
    Fixture f;
    f.fake->script = {httpErr(429), httpOk(okBody())};
    std::atomic<bool> cancel{false};
    auto res = f.client.select(baseReq(), &cancel);
    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.attempts, 2);
}

void test_backoff_is_applied_and_increasing() {
    auto fake = std::make_shared<FakeHttpTransport>();
    GeminiClient client(fake);
    GeminiRetryPolicy p;
    p.max_retries = 2;
    p.backoff_ms = {40, 120};
    client.setRetryPolicy(p);
    fake->script = {httpErr(429), httpErr(429), httpOk(okBody())};

    const auto t0 = std::chrono::steady_clock::now();
    auto res = client.select(baseReq());
    const long long elapsed = msSince(t0);

    EXPECT_TRUE(res.ok);
    EXPECT_EQ(res.attempts, 3);
    EXPECT_TRUE(elapsed >= 150);             // 40 + 120 ms (toleransi scheduler)
    std::cout << "    info: total backoff " << elapsed << " ms\n";
}

void test_retry_disabled_policy() {
    Fixture f;
    GeminiRetryPolicy p;
    p.max_retries = 0;
    p.backoff_ms = {1};
    p.malformed_json_max_retries = 0;
    f.client.setRetryPolicy(p);
    f.fake->script = {httpErr(429), httpOk(okBody())};
    auto res = f.client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::RateLimited);
    EXPECT_EQ(res.attempts, 1);
}

void test_null_transport_is_safe() {
    GeminiClient client(nullptr);
    auto res = client.select(baseReq());
    EXPECT_TRUE(!res.ok);
    EXPECT_EQ(res.error.kind, GeminiErrorKind::Unknown);
    EXPECT_EQ(res.attempts, 0);
}

} // namespace

// ---------------------------------------------------------------- runner

int main(int argc, char** argv) {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"request_json_structure",              test_request_json_structure},
        {"success_parses_selection",            test_success_parses_selection},
        {"success_from_code_fence",             test_success_from_code_fence},
        {"success_from_prose_wrapper",          test_success_from_prose_wrapper},
        {"success_multi_parts_bad_priority",    test_success_multi_parts_and_bad_priority},
        {"success_empty_files_is_valid",        test_success_empty_files_is_valid},
        {"malformed_retries_once_then_fails",   test_malformed_json_retries_once_then_fails},
        {"malformed_then_success",              test_malformed_json_then_success},
        {"missing_files_field_is_malformed",    test_missing_files_field_is_malformed},
        {"broken_envelope_is_malformed",        test_broken_envelope_is_malformed},
        {"empty_text_is_malformed",             test_empty_text_is_malformed},
        {"http_400_no_retry",                   test_http_400_no_retry},
        {"http_401_403_no_retry",               test_http_401_and_403_no_retry},
        {"http_404_model_not_found",            test_http_404_model_not_found},
        {"http_429_then_success",               test_http_429_then_success},
        {"http_429_persistent",                 test_http_429_persistent_exhausts_retries},
        {"http_503_then_success",               test_http_503_then_success},
        {"http_500_persistent",                 test_http_500_persistent},
        {"network_error_then_success",          test_network_error_then_success},
        {"network_error_persistent",            test_network_error_persistent},
        {"timeout_no_retry",                    test_timeout_no_retry},
        {"unknown_http_status_no_retry",        test_unknown_http_status_no_retry},
        {"prompt_feedback_blocked",             test_prompt_feedback_blocked},
        {"finish_safety_blocked",               test_finish_reason_safety_blocked},
        {"finish_max_tokens_truncated",         test_finish_reason_max_tokens_truncated},
        {"no_candidates_is_unknown",            test_no_candidates_is_unknown},
        {"cancel_before_request",               test_cancel_before_request},
        {"cancel_during_backoff",               test_cancel_during_backoff_uses_default_2s},
        {"cancel_during_transport",             test_cancel_during_transport},
        {"no_cancel_completes",                 test_no_cancel_completes_normally},
        {"backoff_increasing",                  test_backoff_is_applied_and_increasing},
        {"retry_disabled_policy",               test_retry_disabled_policy},
        {"null_transport_is_safe",              test_null_transport_is_safe},
    };

    std::string filter;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--list") {
            for (const auto& t : tests) std::cout << t.first << "\n";
            return 0;
        }
        if (a == "--help" || a == "-h") {
            std::cout << "usage: frans_gemini_tests [substring-filter] [--list]\n";
            return 0;
        }
        filter = a;
    }

    int run = 0, failed = 0;
    std::cout << "== frans_gemini_tests (Fase C #56, offline) ==\n";
    for (const auto& t : tests) {
        if (!filter.empty() && t.first.find(filter) == std::string::npos) continue;
        ++run;
        g_current = t.first;
        const int before = g_checks_failed;
        std::cout << "  [RUN ] " << t.first << "\n";
        try {
            t.second();
        } catch (const std::exception& e) {
            ++g_checks_failed;
            std::cout << "    EXCEPTION: " << e.what() << "\n";
        } catch (...) {
            ++g_checks_failed;
            std::cout << "    EXCEPTION: unknown\n";
        }
        if (g_checks_failed > before) {
            ++failed;
            std::cout << "  [FAIL] " << t.first << "\n";
        } else {
            std::cout << "  [ OK ] " << t.first << "\n";
        }
    }

    std::cout << "== " << (run - failed) << "/" << run << " test lulus, "
              << g_checks_failed << " assertion gagal ==\n";
    if (run == 0) {
        std::cout << "Tidak ada test yang cocok dengan filter: " << filter << "\n";
        return 2;
    }
    return failed == 0 ? 0 : 1;
}
