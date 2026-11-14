#pragma once

#include "IHttpTransport.h"

namespace frans::infrastructure::http {

// Implementasi IHttpTransport menggunakan curl CLI via subprocess.
// Keuntungan:
// - Tidak perlu dependensi native baru (curl sudah ada di Windows 10/11 dan Linux)
// - Cancel dan timeout gratis dari ProcessExecutor
// - API key aman: dikirim via stdin, tidak muncul di argv/log
class CurlTransport : public IHttpTransport {
public:
    HttpResponse post(const HttpRequest& req,
                      std::atomic<bool>* cancel = nullptr) override;
    HttpResponse get(const HttpRequest& req,
                     std::atomic<bool>* cancel = nullptr) override;

private:
    HttpResponse execute(const HttpRequest& req,
                         const std::string& method,
                         std::atomic<bool>* cancel);
};

} // namespace frans::infrastructure::http