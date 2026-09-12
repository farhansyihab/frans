// ~/AgenAI/frans/src/infrastructure/system/MemoryInfo.cpp
#include "infrastructure/system/MemoryInfo.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace frans::infrastructure {

namespace {

bool tryParse(const std::string& line, const char* key, std::uint64_t& out) {
    const std::size_t klen = std::strlen(key);
    if (line.size() < klen) return false;
    if (line.compare(0, klen, key) != 0) return false;

    std::istringstream iss(line.substr(klen));
    std::uint64_t v = 0;
    if (!(iss >> v)) return false;
    out = v;
    return true;
}

} // namespace

bool readMemoryInfo(MemoryInfo& out) {
    std::ifstream f("/proc/meminfo");
    if (!f) return false;

    std::string line;
    while (std::getline(f, line)) {
        tryParse(line, "MemTotal:",     out.mem_total_kb);
        tryParse(line, "MemFree:",      out.mem_free_kb);
        tryParse(line, "MemAvailable:", out.mem_available_kb);
        tryParse(line, "Buffers:",      out.buffers_kb);
        tryParse(line, "Cached:",       out.cached_kb);
        tryParse(line, "SwapTotal:",    out.swap_total_kb);
        tryParse(line, "SwapFree:",     out.swap_free_kb);
    }
    return true;
}

std::string MemoryInfo::toHuman() const {
    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "RAM: %.2f GB total / %.2f GB available  |  "
        "Swap: %.2f GB total / %.2f GB free",
        mem_total_gb(),
        mem_available_gb(),
        static_cast<double>(swap_total_kb) / 1024.0 / 1024.0,
        static_cast<double>(swap_free_kb)  / 1024.0 / 1024.0);
    return buf;
}

} // namespace frans::infrastructure
