// ~/AgenAI/frans/src/config/LlamaServerConfig.h
#pragma once

#include <string>

namespace frans::config {

// Blok "llama_server" dari config/providers.json (blueprint §5.2).
// Loader ini tidak throw — kalau file hilang / field kosong, nilai
// default dipakai supaya aplikasi tetap bisa start untuk diagnostik.
struct LlamaServerConfig {
    std::string binary;
    std::string host = "127.0.0.1";
    int port = 8080;
    int startup_timeout_sec = 120;

    static LlamaServerConfig loadFromFile(const std::string& path);
};

} // namespace frans::config
