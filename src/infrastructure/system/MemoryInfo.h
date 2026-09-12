// ~/AgenAI/frans/src/infrastructure/system/MemoryInfo.h
#pragma once

#include <cstdint>
#include <string>

namespace frans::infrastructure {

// Snapshot /proc/meminfo. Nilai dalam KB (sesuai format kernel).
// Catatan: pada laptop dengan iGPU (Vega), MemTotal di sini adalah RAM yang
// TERLIHAT OS — sudah dikurangi shared memory iGPU (blueprint §1).
struct MemoryInfo {
    std::uint64_t mem_total_kb     = 0;
    std::uint64_t mem_free_kb      = 0;
    std::uint64_t mem_available_kb = 0;  // angka paling relevan untuk ResourceGuard
    std::uint64_t buffers_kb       = 0;
    std::uint64_t cached_kb        = 0;
    std::uint64_t swap_total_kb    = 0;
    std::uint64_t swap_free_kb     = 0;

    double mem_total_gb() const     { return static_cast<double>(mem_total_kb)     / 1024.0 / 1024.0; }
    double mem_available_gb() const { return static_cast<double>(mem_available_kb) / 1024.0 / 1024.0; }
    double swap_used_kb() const     { return swap_total_kb > swap_free_kb ? static_cast<double>(swap_total_kb - swap_free_kb) : 0.0; }

    std::string toHuman() const;
};

// Parse /proc/meminfo. Return false kalau file tidak bisa dibuka.
bool readMemoryInfo(MemoryInfo& out);

} // namespace frans::infrastructure
