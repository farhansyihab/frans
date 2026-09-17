#pragma once

#include <string>

namespace frans::config {

struct AppConfig {
    int         window_width     = 0;
    int         window_height    = 0;
    int         window_margin_px = 24;
    std::string window_title     = "Frans";

    std::string default_project_root;

    int build_timeout_sec = 120;
    int test_timeout_sec  = 120;
    int run_timeout_sec   = 30;

    // Baca config/app.json kalau ada. Kalau tidak ada, default dipakai.
    static AppConfig loadFromFile(const std::string& path);
};

} // namespace frans::config