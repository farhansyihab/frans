#pragma once
// Fase C #56 -- test double untuk IHttpTransport.
// Tidak menyentuh jaringan sama sekali: respons diambil dari "script" berurutan.
//
// Catatan: bila field HttpRequest/HttpResponse di proyek berbeda nama,
// sesuaikan bagian buildResponse() / pencatatan request di bawah.

#include "infrastructure/http/IHttpTransport.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace frans::test {

struct FakeStep {
    int status = 200;
    std::string body;
    std::string error;
    bool cancelled = false;
    bool timed_out = false;
    int delay_ms = 0;        // simulasi latensi (peka cancel)
};

class FakeHttpTransport : public infrastructure::http::IHttpTransport {
public:
    std::vector<FakeStep> script;
    std::vector<infrastructure::http::HttpRequest> requests;
    int post_calls = 0;
    int get_calls = 0;
    int cancel_seen = 0;     // berapa kali post()/get() melihat cancel aktif

    ~FakeHttpTransport() override = default;

    infrastructure::http::HttpResponse post(
        const infrastructure::http::HttpRequest& req,
        std::atomic<bool>* cancel) override
    {
        ++post_calls;
        requests.push_back(req);
        return executeStep(cancel);
    }

    infrastructure::http::HttpResponse get(
        const infrastructure::http::HttpRequest& req,
        std::atomic<bool>* cancel) override
    {
        ++get_calls;
        requests.push_back(req);
        return executeStep(cancel);
    }

private:
    infrastructure::http::HttpResponse executeStep(std::atomic<bool>* cancel) {
        const size_t idx = static_cast<size_t>(post_calls + get_calls - 1);
        FakeStep step;
        if (idx < script.size()) {
            step = script[idx];
        } else if (!script.empty()) {
            step = script.back();   // ulang langkah terakhir bila script habis
        } else {
            step.status = 500;
            step.body = R"({"error":{"message":"no script"}})";
        }

        // Simulasi latensi yang bisa diputus.
        int slept = 0;
        while (slept < step.delay_ms) {
            if (cancel && cancel->load()) {
                ++cancel_seen;
                infrastructure::http::HttpResponse r;
                r.status = 0;
                r.cancelled = true;
                r.error = "cancelled by user";
                return r;
            }
            const int slice = std::min(10, step.delay_ms - slept);
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
            slept += slice;
        }

        if (cancel && cancel->load()) ++cancel_seen;

        infrastructure::http::HttpResponse r;
        r.status = step.status;
        r.body = step.body;
        r.error = step.error;
        r.cancelled = step.cancelled;
        r.timed_out = step.timed_out;
        return r;
    }
};

} // namespace frans::test