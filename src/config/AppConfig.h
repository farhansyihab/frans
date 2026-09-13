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
    //
    // window_width / window_height:
    //   0  → AUTO: pakai Fl::screen_work_area() dikurangi 2*margin_px.
    //   >0 → pakai nilai itu, tapi di-clamp ke work area - 2*margin_px
    //        supaya tidak ada bagian window di luar layar.
    //
    // window_margin_px: jarak window dari tepi work area (kiri/kanan/
    // atas/bawah). Berlaku untuk mode AUTO maupun clamp.
    int         window_width     = 0;
    int         window_height    = 0;
    int         window_margin_px = 24;
    std::string window_title     = "frans_ai";

    // Baca config/app.json. Kalau file tidak ada atau field hilang,
    // nilai default di atas dipakai (no-throw).
    static AppConfig loadFromFile(const std::string& path);
};

} // namespace frans::config
