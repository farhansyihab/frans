// ~/AgenAI/frans/src/infrastructure/llama/ModelRegistry.cpp
#include "infrastructure/llama/ModelRegistry.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace frans::infrastructure {

bool ModelEntry::pathExists() const {
    std::error_code ec;
    return !path.empty() && std::filesystem::exists(path, ec);
}

ModelRegistry ModelRegistry::loadFromFile(const std::string& path) {
    ModelRegistry reg;

    std::ifstream f(path);
    if (!f) return reg;

    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception&) {
        return reg;
    }

    if (!j.contains("models") || !j["models"].is_array()) {
        return reg;
    }

    for (const auto& item : j["models"]) {
        if (!item.is_object()) continue;

        ModelEntry e;
        if (item.contains("id")           && item["id"].is_string())           e.id           = item["id"].get<std::string>();
        if (item.contains("path")         && item["path"].is_string())         e.path         = item["path"].get<std::string>();
        if (item.contains("role")         && item["role"].is_string())         e.role         = item["role"].get<std::string>();
        if (item.contains("context_size") && item["context_size"].is_number_integer()) e.context_size = item["context_size"].get<int>();
        if (item.contains("threads")      && item["threads"].is_number_integer())      e.threads      = item["threads"].get<int>();
        if (item.contains("size_gb")      && item["size_gb"].is_number())              e.size_gb      = item["size_gb"].get<double>();
        if (item.contains("source")       && item["source"].is_string())               e.source       = item["source"].get<std::string>();

        // Optional architecture block — dibutuhkan ResourceGuard untuk
        // hitung KV cache eksak (blueprint §5.4).
        if (item.contains("architecture") && item["architecture"].is_object()) {
            const auto& a = item["architecture"];
            if (a.contains("layers")   && a["layers"].is_number_integer())   e.layers   = a["layers"].get<int>();
            if (a.contains("kv_heads") && a["kv_heads"].is_number_integer()) e.kv_heads = a["kv_heads"].get<int>();
            if (a.contains("head_dim") && a["head_dim"].is_number_integer()) e.head_dim = a["head_dim"].get<int>();
        }

        if (!e.id.empty()) {
            reg.models_.push_back(std::move(e));
        }
    }

    return reg;
}

const ModelEntry* ModelRegistry::findById(const std::string& id) const {
    for (const auto& m : models_) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

const ModelEntry* ModelRegistry::findFirstByRole(const std::string& role) const {
    for (const auto& m : models_) {
        if (m.role == role) return &m;
    }
    return nullptr;
}

} // namespace frans::infrastructure
