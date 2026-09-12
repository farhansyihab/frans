// ~/AgenAI/frans/src/infrastructure/llama/ModelRegistry.h
#pragma once

#include <string>
#include <vector>

namespace frans::infrastructure {

// Satu entri dari config/models.json.
//
// Field "architecture" opsional — kalau tersedia, ResourceGuard bisa hitung
// KV cache secara eksak. Kalau tidak, fallback estimasi kasar dipakai.
struct ModelEntry {
    std::string id;
    std::string path;
    std::string role;
    int         context_size = 4096;
    int         threads      = 4;
    double      size_gb      = 0.0;
    std::string source;

    // Architecture (opsional, dari blok "architecture" di models.json).
    int         layers       = 0;
    int         kv_heads     = 0;
    int         head_dim     = 0;

    bool pathExists() const;
    bool hasArchitecture() const {
        return layers > 0 && kv_heads > 0 && head_dim > 0;
    }
};

// Blueprint §5.2 — IModelRegistry (versi minimal, static loader).
// Baca config/models.json, sediakan list & lookup.
class ModelRegistry {
public:
    static ModelRegistry loadFromFile(const std::string& path);

    const std::vector<ModelEntry>& models() const { return models_; }
    bool empty() const { return models_.empty(); }

    const ModelEntry* findById(const std::string& id) const;
    const ModelEntry* findFirstByRole(const std::string& role) const;

private:
    std::vector<ModelEntry> models_;
};

} // namespace frans::infrastructure
