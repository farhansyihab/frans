#include "config/AppConfig.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace frans::config {

namespace {

// Parser JSON super sederhana: cukup untuk config flat.
// Format yang didukung: { "key": value, "key2": value2 }
// Nilai: string, integer. Tidak support nested object.
// Untuk config yang lebih kompleks, ganti dengan nlohmann/json nanti.

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r')) ++a;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r')) --b;
    return s.substr(a, b - a);
}

bool parseInt(const std::string& s, int& out) {
    try { out = std::stoi(s); return true; } catch (...) { return false; }
}

} // namespace

AppConfig AppConfig::loadFromFile(const std::string& path) {
    AppConfig cfg;

    std::ifstream f(path);
    if (!f) return cfg;

    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();

    // Cari pasangan "key" : value sederhana. Abaikan nested.
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t kq = text.find('"', pos);
        if (kq == std::string::npos) break;
        const std::size_t kq2 = text.find('"', kq + 1);
        if (kq2 == std::string::npos) break;

        const std::string key = text.substr(kq + 1, kq2 - kq - 1);

        const std::size_t colon = text.find(':', kq2 + 1);
        if (colon == std::string::npos) break;

        // Cari akhir nilai (koma atau } di level yang sama — kita sederhanakan).
        std::size_t end = colon + 1;
        int depth = 0;
        bool in_str = false;
        while (end < text.size()) {
            const char c = text[end];
            if (c == '"' && (end == 0 || text[end-1] != '\\')) in_str = !in_str;
            if (!in_str) {
                if (c == '{' || c == '[') ++depth;
                else if (c == '}' || c == ']') {
                    if (depth == 0) break;
                    --depth;
                } else if (c == ',' && depth == 0) break;
            }
            ++end;
        }
        const std::string value_raw = trim(text.substr(colon + 1, end - colon - 1));

        if (value_raw.size() >= 2 && value_raw.front() == '"' && value_raw.back() == '"') {
            const std::string v = value_raw.substr(1, value_raw.size() - 2);
            if (key == "window_title")   cfg.window_title = v;
            if (key == "default_project_root") cfg.default_project_root = v;
        } else {
            int iv = 0;
            if (parseInt(value_raw, iv)) {
                if (key == "width")              cfg.window_width = iv;
                if (key == "height")             cfg.window_height = iv;
                if (key == "margin_px")          cfg.window_margin_px = iv;
                if (key == "build_timeout_sec")  cfg.build_timeout_sec = iv;
                if (key == "test_timeout_sec")   cfg.test_timeout_sec = iv;
                if (key == "run_timeout_sec")    cfg.run_timeout_sec = iv;
            }
        }

        pos = (end < text.size()) ? end + 1 : text.size();
    }

    return cfg;
}

} // namespace frans::config