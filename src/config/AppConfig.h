#pragma once
#include <string>
namespace frans::config {
struct AppConfig {
    // Window
    int         window_width     = 0;
    int         window_height    = 0;
    int         window_margin_px = 24;
    std::string window_title     = "Frans";
    std::string default_project_root;  // <-- INI YANG HILANG

    // Build timeouts
    int build_timeout_sec = 120;
    int test_timeout_sec  = 120;
    int run_timeout_sec   = 30;

    // --- Baru untuk v3 ---
    // Ingest
    std::string ingest_output_dir    = "~/frans-out";
    int         ingest_max_file_kb   = 256;
    int         ingest_max_total_mb  = 5;

    // Prompt
    std::string prompt_template_path = "config/prompt_template.txt";

    // Context / Gemini selector
    std::string selector_prompt_path      = "config/gemini_selector_prompt.txt";
    std::string context_output_name       = "SourceCode.md";
    int         context_max_kb            = 300;
    int         context_max_files         = 40;
    std::string context_always_include    = "BluePrint.md";

    // Gemini
    std::string gemini_model              = "gemini-2.5-flash";
    std::string gemini_endpoint           = "https://generativelanguage.googleapis.com/v1beta";
    int         gemini_timeout_sec        = 90;
    int         gemini_full_mode_max_tokens = 150000;
    int         gemini_index_head_lines   = 40;
    std::string gemini_api_key_file;

    static AppConfig loadFromFile(const std::string& path);
};
} // namespace frans::config
