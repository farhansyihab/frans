// ~/AgenAI/frans/src/application/resource_guard/ResourceGuard.cpp
#include "application/resource_guard/ResourceGuard.h"

#include "infrastructure/system/MemoryInfo.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace frans::application {

using frans::infrastructure::MemoryInfo;
using frans::infrastructure::readMemoryInfo;

namespace {

constexpr double kBytesToGb         = 1024.0 * 1024.0 * 1024.0;
constexpr double kBytesPerElem      = 2.0;   // FP16 — llama.cpp default KV cache dtype
constexpr double kRuntimeOverheadGb = 0.5;   // proses llama-server + buffer + mmap

// Batas bawah headroom setelah semua kebutuhan. Di bawah ini → langsung Reject,
// karena OS perlu ruang bernapas (page cache, GC, dsb).
constexpr double kBareMinimumHeadroomGb = 0.3;

std::string formatGb(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f GB", v);
    return std::string(buf);
}

} // namespace

// ---------------------------------------------------------------------------
// KV cache formula (blueprint §5.4)
// ---------------------------------------------------------------------------
double computeKvCacheGb(int layers, int kv_heads, int head_dim, int context_size) {
    if (layers <= 0 || kv_heads <= 0 || head_dim <= 0 || context_size <= 0) {
        return 0.0;
    }
    const double bytes = 2.0 * static_cast<double>(layers)
                       * static_cast<double>(kv_heads)
                       * static_cast<double>(head_dim)
                       * kBytesPerElem
                       * static_cast<double>(context_size);
    return bytes / kBytesToGb;
}

// ---------------------------------------------------------------------------
// ResourceGuard
// ---------------------------------------------------------------------------
ResourceGuard::ResourceGuard(const frans::config::AppConfig& cfg)
    : cfg_(cfg) {}

ModelRequirement ResourceGuard::estimateModelRequirement(const ModelEntry& entry,
                                                         int context_size) const {
    ModelRequirement req;
    req.model_id            = entry.id;
    req.context_size        = context_size;
    req.model_file_gb       = entry.size_gb;
    req.runtime_overhead_gb = kRuntimeOverheadGb;
    req.layers              = entry.layers;
    req.kv_heads            = entry.kv_heads;
    req.head_dim            = entry.head_dim;

    if (entry.hasArchitecture()) {
        req.kv_cache_gb  = computeKvCacheGb(entry.layers, entry.kv_heads,
                                            entry.head_dim, context_size);
        req.kv_estimated = true;
    } else {
        // Fallback: tebakan kasar (~5% ukuran file per 4096 context).
        // Cukup supaya tidak crash, tapi user diberi tahu lewat flag.
        req.kv_cache_gb  = entry.size_gb * 0.05 * (static_cast<double>(context_size) / 4096.0);
        req.kv_estimated = false;
    }

    req.total_required_gb = req.model_file_gb + req.kv_cache_gb
                          + req.runtime_overhead_gb;
    return req;
}

double ResourceGuard::availableGb() const {
    MemoryInfo mem;
    if (!readMemoryInfo(mem)) return 0.0;
    return mem.mem_available_gb();
}

ResourceCheck ResourceGuard::check(const ModelEntry& entry, int context_size) const {
    ResourceCheck out;
    out.requirement = estimateModelRequirement(entry, context_size);

    MemoryInfo mem;
    if (!readMemoryInfo(mem)) {
        out.decision    = ResourceDecision::Reject;
        out.explanation = "Tidak bisa membaca /proc/meminfo.";
        out.suggestion  = "Cek permission /proc/meminfo.";
        return out;
    }

    out.available_gb = mem.mem_available_gb();

    const double safety_margin_gb = cfg_.ram_available_threshold_mb / 1024.0;
    const double required         = out.requirement.total_required_gb;
    const double headroom         = out.available_gb - required;

    char buf[384];

    if (headroom >= safety_margin_gb) {
        out.decision = ResourceDecision::Proceed;
        std::snprintf(buf, sizeof(buf),
            "RAM cukup. Butuh %s  (model %s + KV %s + overhead %s). "
            "Tersedia %s, headroom %s (>= safety margin %s).",
            formatGb(required).c_str(),
            formatGb(out.requirement.model_file_gb).c_str(),
            formatGb(out.requirement.kv_cache_gb).c_str(),
            formatGb(out.requirement.runtime_overhead_gb).c_str(),
            formatGb(out.available_gb).c_str(),
            formatGb(headroom).c_str(),
            formatGb(safety_margin_gb).c_str());
        out.explanation = buf;
        return out;
    }

    if (headroom >= kBareMinimumHeadroomGb) {
        out.decision = ResourceDecision::CleanMemoryThenProceed;
        std::snprintf(buf, sizeof(buf),
            "RAM mepet. Butuh %s, tersedia %s, headroom hanya %s "
            "(< safety margin %s). Disarankan cleanMemory dulu.",
            formatGb(required).c_str(),
            formatGb(out.available_gb).c_str(),
            formatGb(headroom).c_str(),
            formatGb(safety_margin_gb).c_str());
        out.explanation = buf;
        return out;
    }

    // Reject
    out.decision = ResourceDecision::Reject;
    std::snprintf(buf, sizeof(buf),
        "RAM tidak cukup. Butuh %s, tersedia hanya %s (defisit %s).",
        formatGb(required).c_str(),
        formatGb(out.available_gb).c_str(),
        formatGb(std::fabs(headroom)).c_str());
    out.explanation = buf;

    std::string suggest;
    if (context_size > 2048) {
        char s[160];
        std::snprintf(s, sizeof(s),
            "Coba context lebih kecil (mis. %d) — KV cache turun ~%s. ",
            context_size / 2,
            formatGb(out.requirement.kv_cache_gb / 2.0).c_str());
        suggest += s;
    }
    suggest += "Atau pilih model dengan role 'coder-light' (3B).";
    out.suggestion = suggest;

    return out;
}

} // namespace frans::application
