// ~/AgenAI/frans/src/config/AppConfig.h
#pragma once

#include <string>

namespace frans::config {

// Konfigurasi global aplikasi (blueprint §4 — Singleton diakses lewat IConfig).
// Untuk tahap ini disimpan sebagai POD sederhana; nanti akan dibungkus
// interface IConfig + Meyers' Singleton tanpa mengubah pemakai.
struct AppConfig {
    // Resource guard (blueprint §5.4)
    int    ram_available_threshold_mb = 1500;
    int    idle_unload_minutes        = 5;

    // Biaya API berbayar (blueprint §5.3)
    double daily_api_budget_usd       = 1.0;

    // Lokasi log
    std::string log_dir;

    // UI
    int         window_width   = 1000;
    int         window_height  = 650;
    std::string window_title   = "frans_ai";

    // Baca config/app.json. Kalau file tidak ada atau field hilang,
    // nilai default di atas dipakai (no-throw).
    static AppConfig loadFromFile(const std::string& path);
};

} // namespace frans::config
