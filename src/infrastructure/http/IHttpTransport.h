#pragma once

#include <atomic>
#include <string>
#include <vector>

namespace frans::infrastructure::http {

// Request yang akan dikirim via curl.
// headers_secret: header yang mengandung data sensitif (API key, dll).
//                 Dikirim via stdin curl config (-K -) agar tidak muncul di ps/Task Manager.
// headers_plain: header biasa, dikirim via argv -H (boleh terlihat di process list).
struct HttpRequest {
    std::string url;
    std::vector<std::string> headers_secret;
    std::vector<std::string> headers_plain;
    std::string body;
    int timeout_sec = 90;
};

struct HttpResponse {
    int status = 0;
    std::string body;
    std::string error;
    bool cancelled = false;
    bool timed_out = false;
};

class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;
    virtual HttpResponse post(const HttpRequest& req,
                              std::atomic<bool>* cancel = nullptr) = 0;
    virtual HttpResponse get(const HttpRequest& req,
                             std::atomic<bool>* cancel = nullptr) = 0;
};

} // namespace frans::infrastructure::http