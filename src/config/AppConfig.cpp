// ~/AgenAI/frans/src/config/AppConfig.cpp
#include "config/AppConfig.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace frans::config {

AppConfig AppConfig::loadFromFile(const std::string& path) {
    AppConfig cfg;

    std::ifstream f(path);
    if (!f) {
        // File tidak ada -> pakai default. Bukan error fatal.
        return cfg;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception&) {
        // JSON rusak -> pakai default. Nanti bisa di-log ke file.
        return cfg;
    }

    if (j.contains("ram_available_threshold_mb") && j["ram_available_threshold_mb"].is_number_integer())
        cfg.ram_available_threshold_mb = j["ram_available_threshold_mb"].get<int>();

    if (j.contains("idle_unload_minutes") && j["idle_unload_minutes"].is_number_integer())
        cfg.idle_unload_minutes = j["idle_unload_minutes"].get<int>();

    if (j.contains("daily_api_budget_usd") && j["daily_api_budget_usd"].is_number())
        cfg.daily_api_budget_usd = j["daily_api_budget_usd"].get<double>();

    if (j.contains("log_dir") && j["log_dir"].is_string())
        cfg.log_dir = j["log_dir"].get<std::string>();

    if (j.contains("window") && j["window"].is_object()) {
        const auto& w = j["window"];
        if (w.contains("width")      && w["width"].is_number_integer())      cfg.window_width      = w["width"].get<int>();
        if (w.contains("height")     && w["height"].is_number_integer())     cfg.window_height     = w["height"].get<int>();
        if (w.contains("margin_px")  && w["margin_px"].is_number_integer())  cfg.window_margin_px  = w["margin_px"].get<int>();
        if (w.contains("title")      && w["title"].is_string())              cfg.window_title      = w["title"].get<std::string>();
    }

    return cfg;
}

} // namespace frans::config
