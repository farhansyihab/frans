// ~/AgenAI/frans/src/config/DeepSeekConfig.cpp
#include "config/DeepSeekConfig.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace frans::config {

namespace {

// Baca satu key string dari env. Return "" kalau tidak ada / kosong.
std::string readEnv(const std::string& name) {
    if (name.empty()) return {};
    const char* v = std::getenv(name.c_str());
    if (!v || !*v) return {};
    return std::string(v);
}

// Baca file secrets.json; cari key "deepseek_api_key". Return "" kalau
// file tidak ada, JSON rusak, atau key tidak ada. Tidak throw.
std::string readKeyFromSecretsFile(const std::string& path) {
    if (path.empty()) return {};

    std::ifstream f(path);
    if (!f) return {};

    nlohmann::json j;
    try {
        f >> j;
    } catch (...) {
        return {};
    }

    if (!j.is_object()) return {};
    if (!j.contains("deepseek_api_key")) return {};
    if (!j["deepseek_api_key"].is_string()) return {};

    const std::string k = j["deepseek_api_key"].get<std::string>();
    return k;   // boleh kosong — caller akan treat sebagai tidak ada
}

} // namespace

std::string DeepSeekConfig::defaultSecretsPath() {
    const char* home = std::getenv("HOME");
    if (!home || !*home) return {};
    return std::string(home) + "/.config/frans/secrets.json";
}

DeepSeekConfig DeepSeekConfig::loadFromFile(const std::string& path) {
    DeepSeekConfig cfg;

    // ---- 1. Parse providers.json ----
    {
        std::ifstream f(path);
        if (!f) {
            // File tidak ada -> config tetap kosong (caller akan sadar
            // api_key kosong dan menolak).
            std::fprintf(stderr,
                "[frans][deepseek] providers.json tidak ditemukan: %s\n",
                path.c_str());
            std::fflush(stderr);
        } else {
            nlohmann::json j;
            try {
                f >> j;
            } catch (const std::exception& e) {
                std::fprintf(stderr,
                    "[frans][deepseek] JSON parse error: %s\n", e.what());
                std::fflush(stderr);
            }

            if (j.contains("deepseek") && j["deepseek"].is_object()) {
                const auto& d = j["deepseek"];

                if (d.contains("endpoint") && d["endpoint"].is_string())
                    cfg.endpoint = d["endpoint"].get<std::string>();

                if (d.contains("model") && d["model"].is_string())
                    cfg.model = d["model"].get<std::string>();

                if (d.contains("api_key_env") && d["api_key_env"].is_string())
                    cfg.api_key_env = d["api_key_env"].get<std::string>();

                if (d.contains("input_cost_per_mtok_usd")
                    && d["input_cost_per_mtok_usd"].is_number())
                    cfg.input_cost_per_mtok_usd =
                        d["input_cost_per_mtok_usd"].get<double>();

                if (d.contains("output_cost_per_mtok_usd")
                    && d["output_cost_per_mtok_usd"].is_number())
                    cfg.output_cost_per_mtok_usd =
                        d["output_cost_per_mtok_usd"].get<double>();

                if (d.contains("connect_timeout_sec")
                    && d["connect_timeout_sec"].is_number_integer())
                    cfg.connect_timeout_sec =
                        d["connect_timeout_sec"].get<int>();

                if (d.contains("read_timeout_sec")
                    && d["read_timeout_sec"].is_number_integer())
                    cfg.read_timeout_sec =
                        d["read_timeout_sec"].get<int>();
            }
        }
    }

    // ---- 2. Resolve api_key: env var dulu ----
    cfg.api_key = readEnv(cfg.api_key_env);

    // ---- 3. Fallback: ~/.config/frans/secrets.json ----
    if (cfg.api_key.empty()) {
        const std::string secrets = defaultSecretsPath();
        cfg.api_key = readKeyFromSecretsFile(secrets);
        if (!cfg.api_key.empty()) {
            std::fprintf(stderr,
                "[frans][deepseek] api_key dibaca dari %s\n",
                secrets.c_str());
            std::fflush(stderr);
        }
    }

    if (cfg.api_key.empty()) {
        std::fprintf(stderr,
            "[frans][deepseek] PERINGATAN: api_key tidak ditemukan. "
            "Set env %s atau isi 'deepseek_api_key' di %s\n",
            cfg.api_key_env.empty()
                ? "(api_key_env tidak diset di providers.json)"
                : cfg.api_key_env.c_str(),
            defaultSecretsPath().c_str());
        std::fflush(stderr);
    }

    return cfg;
}

} // namespace frans::config
