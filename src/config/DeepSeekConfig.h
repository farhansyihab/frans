// ~/AgenAI/frans/src/config/DeepSeekConfig.h
#pragma once

#include <string>

namespace frans::config {

// Konfigurasi DeepSeek dari config/providers.json blok "deepseek".
//
// Field `api_key` TIDAK dibaca dari providers.json — hanya dari:
//   1. Environment variable bernama `api_key_env` (prioritas utama), atau
//   2. File ~/.config/frans/secrets.json dengan key "deepseek_api_key"
//      (fallback, untuk kenyamanan agar tidak perlu export env tiap sesi).
//
// Ini mengikuti blueprint §8 "Keamanan API key": jangan pernah commit key
// plaintext ke repo. providers.json cukup menyebut NAMA env var-nya.
struct DeepSeekConfig {
    // Wajib ada (dari providers.json):
    std::string endpoint;        // URL lengkap, mis. https://api.deepseek.com/chat/completions
    std::string model;           // mis. "deepseek-chat"
    std::string api_key_env;     // NAMA env var, mis. "DEEPSEEK_API_KEY"

    double input_cost_per_mtok_usd  = 0.0;   // $/juta token input
    double output_cost_per_mtok_usd = 0.0;   // $/juta token output

    // Opsional (default kalau tidak ada di JSON):
    int connect_timeout_sec = 10;
    int read_timeout_sec    = 300;    // 5 menit — reasoning bisa panjang

    // Resolved at load time:
    std::string api_key;         // kosong kalau tidak ada env var / file

    bool hasApiKey() const { return !api_key.empty(); }

    // Baca providers.json. Field yang hilang memakai default di atas.
    // Setelah load, langsung resolve api_key dari env / secrets file.
    static DeepSeekConfig loadFromFile(const std::string& path);

    // Path default secrets file (~/.config/frans/secrets.json). Return ""
    // kalau $HOME tidak di-set.
    static std::string defaultSecretsPath();
};

} // namespace frans::config
