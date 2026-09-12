// ~/AgenAI/frans/src/application/resource_guard/ResourceGuard.h
#pragma once

#include "config/AppConfig.h"
#include "infrastructure/llama/ModelRegistry.h"

#include <string>

namespace frans::application {

using frans::infrastructure::ModelEntry;

// Estimasi kebutuhan RAM satu model pada context_size tertentu.
//
// Rumus KV cache (FP16, GQA — dipakai llama.cpp default):
//   KV_bytes = 2 (K+V) * layers * kv_heads * head_dim * 2 byte * context_size
//
// "runtime_overhead" = RAM di luar model & KV cache (proses llama-server,
// buffer I/O, tokenizer, mmap metadata). 0.5 GB adalah nilai empiris
// konservatif untuk llama.cpp build 10927 di mesin ini.
struct ModelRequirement {
    std::string model_id;
    int         context_size        = 0;

    double      model_file_gb       = 0.0;
    double      kv_cache_gb         = 0.0;
    double      runtime_overhead_gb = 0.5;
    double      total_required_gb   = 0.0;

    int         layers              = 0;
    int         kv_heads            = 0;
    int         head_dim            = 0;
    bool        kv_estimated        = false;  // false = architecture tidak tersedia, KV ditebak
};

enum class ResourceDecision {
    Proceed,                 // RAM cukup nyaman (headroom >= safety margin)
    CleanMemoryThenProceed,  // mepet: disarankan cleanMemory dulu
    Reject                   // tidak cukup walau setelah clean
};

struct ResourceCheck {
    ResourceDecision  decision    = ResourceDecision::Reject;
    ModelRequirement  requirement;
    double            available_gb = 0.0;   // MemAvailable saat check
    std::string       explanation;          // human-readable, siap ditampilkan
    std::string       suggestion;           // terisi kalau Reject
};

// Blueprint §5.4 — Resource Guard.
//
// Murni perhitungan + pembacaan /proc/meminfo. TIDAK melakukan aksi apa pun
// (clean memory, start/stop). Caller (UI / Agent) yang memutuskan.
class ResourceGuard {
public:
    explicit ResourceGuard(const frans::config::AppConfig& cfg);

    // Hitung estimasi kebutuhan RAM satu model (file + KV cache + overhead).
    // Aman dipanggil kapan saja, tidak menyentuh /proc/meminfo.
    ModelRequirement estimateModelRequirement(const ModelEntry& entry,
                                              int context_size) const;

    // Baca /proc/meminfo sekarang, lalu tentukan decision memakai
    // cfg.ram_available_threshold_mb sebagai safety margin.
    ResourceCheck check(const ModelEntry& entry, int context_size) const;

    // RAM available saat ini (GB). 0.0 kalau /proc/meminfo tidak terbaca.
    double availableGb() const;

private:
    frans::config::AppConfig cfg_;
};

// Ekspos rumus KV cache untuk unit test / tooling eksternal.
double computeKvCacheGb(int layers, int kv_heads, int head_dim, int context_size);

} // namespace frans::application
