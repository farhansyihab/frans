// ~/AgenAI/frans/src/config/GeminiConfig.cpp
#include "config/GeminiConfig.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

namespace frans::config {

namespace {

std::string readEnv(const std::string& name) {
    if (name.empty()) return {};
    const char* v = std::getenv(name.c_str());
    if (!v || !*v) return {};
    return std::string(v);
}

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
    if (!j.contains("gemini_api_key")) return {};
    if (!j["gemini_api_key"].is_string()) return {};

    return j["gemini_api_key"].get<std::string>();
}

} // namespace

std::string GeminiConfig::defaultSecretsPath() {
    const char* home = std::getenv("HOME");
    if (!home || !*home) return {};
    return std::string(home) + "/.config/frans/secrets.json";
}

GeminiConfig GeminiConfig::loadFromFile(const std::string& path) {
    GeminiConfig cfg;

    // ---- 1. Parse providers.json ----
    {
        std::ifstream f(path);
        if (!f) {
            std::fprintf(stderr,
                "[frans][gemini] providers.json tidak ditemukan: %s\n",
                path.c_str());
            std::fflush(stderr);
        } else {
            nlohmann::json j;
            try {
                f >> j;
            } catch (const std::exception& e) {
                std::fprintf(stderr,
                    "[frans][gemini] JSON parse error: %s\n", e.what());
                std::fflush(stderr);
            }

            if (j.contains("gemini") && j["gemini"].is_object()) {
                const auto& g = j["gemini"];

                if (g.contains("enabled") && g["enabled"].is_boolean())
                    cfg.enabled = g["enabled"].get<bool>();

                if (g.contains("endpoint") && g["endpoint"].is_string())
                    cfg.endpoint = g["endpoint"].get<std::string>();

                if (g.contains("model") && g["model"].is_string())
                    cfg.model = g["model"].get<std::string>();

                if (g.contains("api_key_env") && g["api_key_env"].is_string())
                    cfg.api_key_env = g["api_key_env"].get<std::string>();

                // Stage 12 follow-up — thinking budget.
                if (g.contains("thinking_budget")
                    && g["thinking_budget"].is_number_integer())
                    cfg.thinking_budget =
                        g["thinking_budget"].get<int>();

                if (g.contains("input_cost_per_mtok_usd")
                    && g["input_cost_per_mtok_usd"].is_number())
                    cfg.input_cost_per_mtok_usd =
                        g["input_cost_per_mtok_usd"].get<double>();

                if (g.contains("output_cost_per_mtok_usd")
                    && g["output_cost_per_mtok_usd"].is_number())
                    cfg.output_cost_per_mtok_usd =
                        g["output_cost_per_mtok_usd"].get<double>();

                if (g.contains("connect_timeout_sec")
                    && g["connect_timeout_sec"].is_number_integer())
                    cfg.connect_timeout_sec =
                        g["connect_timeout_sec"].get<int>();

                if (g.contains("read_timeout_sec")
                    && g["read_timeout_sec"].is_number_integer())
                    cfg.read_timeout_sec =
                        g["read_timeout_sec"].get<int>();
            }
        }
    }

    // ---- 2. Kalau disabled, tidak perlu resolve api_key (hemat I/O) ----
    if (!cfg.enabled) {
        std::fprintf(stderr,
            "[frans][gemini] provider disabled via providers.json "
            "(gemini.enabled=false). Skip resolve api_key.\n");
        std::fflush(stderr);
        return cfg;
    }

    // ---- 3. Resolve api_key: env var dulu ----
    cfg.api_key = readEnv(cfg.api_key_env);

    // ---- 4. Fallback: ~/.config/frans/secrets.json ----
    if (cfg.api_key.empty()) {
        const std::string secrets = defaultSecretsPath();
        cfg.api_key = readKeyFromSecretsFile(secrets);
        if (!cfg.api_key.empty()) {
            std::fprintf(stderr,
                "[frans][gemini] api_key dibaca dari %s\n",
                secrets.c_str());
            std::fflush(stderr);
        }
    }

    if (cfg.api_key.empty()) {
        std::fprintf(stderr,
            "[frans][gemini] PERINGATAN: api_key tidak ditemukan. "
            "Set env %s atau isi 'gemini_api_key' di %s\n",
            cfg.api_key_env.empty()
                ? "(api_key_env tidak diset di providers.json)"
                : cfg.api_key_env.c_str(),
            defaultSecretsPath().c_str());
        std::fflush(stderr);
    }

    return cfg;
}

} // namespace frans::config