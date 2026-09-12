// ~/AgenAI/frans/src/infrastructure/http/HttpProbe.h
#pragma once

#include <string>

namespace frans::infrastructure {

// Hasil satu HTTP GET singkat. Dipakai untuk ready-probe endpoint lokal
// seperti /health pada llama-server (blueprint §5.2 — waitUntilReady).
struct HttpProbeResult {
    bool        reachable   = false;  // response HTTP diterima (bukan cuma TCP connect)
    int         status_code = 0;      // 0 kalau tidak sampai / connection error
    std::string body;                 // response body (kalau ada)
    std::string error;                // pesan error transport (kalau reachable=false)
};

// GET http://host:port/path dengan timeout pendek.
// Cocok untuk health check — BUKAN untuk streaming / request besar.
HttpProbeResult httpGet(const std::string& host,
                        int port,
                        const std::string& path,
                        int timeout_sec = 3);

} // namespace frans::infrastructure
