// ~/AgenAI/frans/src/config/LlamaServerConfig.cpp
#include "config/LlamaServerConfig.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace frans::config {

LlamaServerConfig LlamaServerConfig::loadFromFile(const std::string& path) {
    LlamaServerConfig cfg;

    std::ifstream f(path);
    if (!f) return cfg;

    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception&) {
        return cfg;
    }

    if (!j.contains("llama_server") || !j["llama_server"].is_object()) {
        return cfg;
    }

    const auto& ls = j["llama_server"];
    if (ls.contains("binary")              && ls["binary"].is_string())              cfg.binary              = ls["binary"].get<std::string>();
    if (ls.contains("host")                && ls["host"].is_string())                cfg.host                = ls["host"].get<std::string>();
    if (ls.contains("port")                && ls["port"].is_number_integer())        cfg.port                = ls["port"].get<int>();
    if (ls.contains("startup_timeout_sec") && ls["startup_timeout_sec"].is_number_integer()) cfg.startup_timeout_sec = ls["startup_timeout_sec"].get<int>();

    return cfg;
}

} // namespace frans::config
