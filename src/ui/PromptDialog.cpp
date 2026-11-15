// src/ui/PromptDialog.cpp
// Main file: constructor, destructor, buildUI, UI update helpers
#include "ui/PromptDialog.h"
#include "application/prompt/PromptGenerator.h"
#include "application/ai/ApiKeyProvider.h"
#include "application/context/ContextSelector.h"
#include "application/context/SourceCodeBuilder.h"
#include "infrastructure/fs/SandboxedFileSystem.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Browser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace frans::ui {

namespace {
constexpr int kW      = 720;
constexpr int kH      = 850;
constexpr int kPadX   = 15;
constexpr int kInnerW = kW - 2 * kPadX;
constexpr int kBtnH   = 28;
constexpr int kRowGap = 6;

// ---------------------------------------------------------------------------
// Helper: Resolve project root dengan fallback.
// ---------------------------------------------------------------------------
std::string resolveProjectRoot(SessionState* session,
                               const frans::config::AppConfig& cfg)
{
    if (session && !session->last_project_root.empty()) {
        return session->last_project_root;
    }
    if (!cfg.default_project_root.empty()) {
        std::fprintf(stderr,
                     "[frans][prompt_dialog] session->last_project_root kosong, "
                     "fallback ke default_project_root: %s\n",
                     cfg.default_project_root.c_str());
        std::fflush(stderr);
        return cfg.default_project_root;
    }
    return "";
}

std::string formatBytesHelper(std::size_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        return buf;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

std::string readFileContents(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

// ===========================================================================
// Public
// ===========================================================================
void PromptDialog::show(SessionState* session,
                        const frans::config::AppConfig& cfg)
{
    PromptDialog* dlg = new PromptDialog(session, cfg);
    dlg->win_->set_modal();
    dlg->win_->show();
    while (dlg->win_->shown()) Fl::wait();
    delete dlg;
}

PromptDialog::PromptDialog(SessionState* session,
                           const frans::config::AppConfig& cfg)
    : session_(session), cfg_(cfg)
{
    buildUI();
    updateKeyStatus();
    updateFreshnessDisplay();
}

PromptDialog::~PromptDialog() {
    delete request_buf_;     request_buf_    = nullptr;
    delete error_buf_;       error_buf_      = nullptr;
    delete preview_buf_;     preview_buf_    = nullptr;
    
    if (list_models_worker_running_.load()) {
        list_models_worker_cancel_.store(true);
    }
    if (list_models_worker_thread_.joinable()) list_models_worker_thread_.join();
    
    if (select_worker_running_.load()) {
        select_worker_cancel_.store(true);
    }
    if (select_worker_thread_.joinable()) select_worker_thread_.join();
    
    if (build_worker_thread_.joinable()) build_worker_thread_.join();
    
    if (win_) {
        if (win_->shown()) win_->hide();
        delete win_;
        win_ = nullptr;
    }
}

// ===========================================================================
// UI construction
// ===========================================================================
void PromptDialog::buildUI() {
    win_ = new Fl_Window(kW, kH, "Prompt Generator");
    win_->begin();
    
    int y = kPadX;
    
    {
        Fl_Box* t = new Fl_Box(kPadX, y, kInnerW, 20, "1. Permintaan");
        t->labelfont(FL_BOLD);
        t->labelsize(13);
        t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        y += 22;
    }
    
    {
        Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 16,
                               "Tulis permintaan Anda untuk AI (wajib):");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        l->labelcolor(FL_DARK_BLUE);
        y += 18;
    }
    
    {
        const int editor_h = 100;
        request_editor_ = new Fl_Text_Editor(kPadX, y, kInnerW, editor_h, nullptr);
        request_buf_ = new Fl_Text_Buffer();
        request_editor_->buffer(request_buf_);
        request_editor_->textfont(FL_COURIER);
        request_editor_->textsize(11);
        request_editor_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        request_editor_->color(FL_WHITE);
        y += editor_h + kRowGap;
    }
    
    {
        Fl_Box* l = new Fl_Box(kPadX, y, kInnerW - 180, 18, "Error output (opsional):");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        l->labelcolor(FL_DARK_BLUE);
        fetch_btn_ = new Fl_Button(kPadX + kInnerW - 170, y, 170, 20, "Ambil dari build terakhir");
        fetch_btn_->callback(cbFetchLastError, this);
        y += 22;
    }
    
    {
        const int editor_h = 60;
        error_editor_ = new Fl_Text_Editor(kPadX, y, kInnerW, editor_h, nullptr);
        error_buf_ = new Fl_Text_Buffer();
        error_editor_->buffer(error_buf_);
        error_editor_->textfont(FL_COURIER);
        error_editor_->textsize(10);
        error_editor_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        error_editor_->color(FL_WHITE);
        y += editor_h + kRowGap;
    }
    
    {
        Fl_Box* t = new Fl_Box(kPadX, y, 120, 20, "2. Pilih file");
        t->labelfont(FL_BOLD);
        t->labelsize(13);
        t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        select_btn_ = new Fl_Button(kPadX + 130, y, 180, 22, "Pilih File via Gemini");
        select_btn_->callback(cbSelectFiles, this);
        key_status_box_ = new Fl_Box(kPadX + 320, y, kInnerW - 320, 20, "");
        key_status_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        key_status_box_->labelsize(10);
        y += 24;
    }
    
    {
        mode_box_ = new Fl_Box(kPadX, y, kInnerW - 200, 18, "Mode: - | Token: -");
        mode_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        mode_box_->labelsize(10);
        mode_box_->labelcolor(FL_DARK_GREEN);
        test_conn_btn_ = new Fl_Button(kPadX + kInnerW - 190, y, 190, 20, "Tes Koneksi / Daftar Model");
        test_conn_btn_->callback(cbTestConnection, this);
        y += 22;
    }
    
    {
        const int browser_h = 130;
        file_browser_ = new Fl_Check_Browser(kPadX, y, kInnerW, browser_h);
        file_browser_->callback(cbBrowserChanged, this);
        file_browser_->textfont(FL_COURIER);
        file_browser_->textsize(10);
        y += browser_h + kRowGap;
    }
    
    {
        total_box_ = new Fl_Box(kPadX, y, kInnerW - 280, 20, "Total terpilih: 0 file, 0 B");
        total_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        total_box_->labelsize(11);
        add_manual_btn_ = new Fl_Button(kPadX + kInnerW - 270, y, 130, 22, "+ Tambah manual...");
        add_manual_btn_->callback(cbAddManual, this);
        cancel_select_btn_ = new Fl_Button(kPadX + kInnerW - 130, y, 130, 22, "Batal");
        cancel_select_btn_->callback(cbCancelSelect, this);
        cancel_select_btn_->deactivate();
        y += 24 + kRowGap;
    }
    
    {
        Fl_Box* t = new Fl_Box(kPadX, y, 140, 20, "3. SourceCode.md");
        t->labelfont(FL_BOLD);
        t->labelsize(13);
        t->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        build_btn_ = new Fl_Button(kPadX + 150, y, 180, 22, "Bangun SourceCode.md");
        build_btn_->callback(cbBuildSourceCode, this);
        freshness_box_ = new Fl_Box(kPadX + 340, y, kInnerW - 340, 20, "Status: -");
        freshness_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        freshness_box_->labelsize(11);
        y += 24;
    }
    
    {
        location_box_ = new Fl_Box(kPadX, y, kInnerW - 120, 18, "Lokasi: -");
        location_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        location_box_->labelsize(10);
        open_folder_btn_ = new Fl_Button(kPadX + kInnerW - 110, y, 110, 20, "Buka Folder");
        open_folder_btn_->callback(cbOpenFolder, this);
        open_folder_btn_->deactivate();
        y += 22 + kRowGap;
    }
    
    {
        status_box_ = new Fl_Box(kPadX, y, kInnerW, 22,
                                 "Idle. Tulis permintaan lalu klik [Pilih File via Gemini].");
        status_box_->box(FL_DOWN_BOX);
        status_box_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        status_box_->labelsize(11);
        y += 22 + kRowGap;
    }
    
    {
        generate_btn_ = new Fl_Button(kPadX, y, 160, kBtnH, "Generate Prompt");
        generate_btn_->callback(cbGenerate, this);
        copy_btn_ = new Fl_Button(kPadX + 170, y, 100, kBtnH, "Copy");
        copy_btn_->callback(cbCopy, this);
        copy_btn_->deactivate();
        Fl_Button* close_btn = new Fl_Button(kPadX + kInnerW - 100, y, 100, kBtnH, "Close");
        close_btn->callback(cbClose, this);
        y += kBtnH + kRowGap;
    }
    
    {
        Fl_Box* l = new Fl_Box(kPadX, y, kInnerW, 16, "4. Preview Prompt (read-only):");
        l->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        l->labelsize(11);
        l->labelcolor(FL_DARK_BLUE);
        y += 18;
    }
    
    {
        const int preview_h = kH - y - kPadX;
        preview_display_ = new Fl_Text_Display(kPadX, y, kInnerW, preview_h > 60 ? preview_h : 60, nullptr);
        preview_buf_ = new Fl_Text_Buffer();
        preview_display_->buffer(preview_buf_);
        preview_display_->textfont(FL_COURIER);
        preview_display_->textsize(10);
        preview_display_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        preview_display_->color(FL_WHITE);
    }
    
    win_->end();
    win_->resizable(preview_display_);
}

// ===========================================================================
// Helper methods
// ===========================================================================
std::string PromptDialog::formatBytes(std::size_t bytes) {
    return formatBytesHelper(bytes);
}

std::string PromptDialog::computeRequestHash() {
    if (!request_buf_) return "";
    const char* text = request_buf_->text();
    std::string req = text ? text : "";
    return frans::application::fnv1a_64_hex(req);
}

PromptDialog::Freshness PromptDialog::checkFreshness() {
    if (last_build_result_.manifest_path.empty()) return Freshness::NoFile;
    
    std::ifstream f(last_build_result_.manifest_path);
    if (!f) return Freshness::NoFile;
    
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string manifest_text = ss.str();
    
    try {
        auto manifest = nlohmann::json::parse(manifest_text);
        std::string manifest_request_hash = manifest.value("request_hash", "");
        std::string current_hash = computeRequestHash();
        
        if (manifest_request_hash != current_hash) return Freshness::Stale;
        
        auto files = manifest["files"];
        if (!files.is_array()) return Freshness::Stale;
        
        std::string project_root = resolveProjectRoot(session_, cfg_);
        if (project_root.empty()) return Freshness::Stale;
        
        infrastructure::SandboxedFileSystem fs(project_root);
        
        for (const auto& file_entry : files) {
            std::string path = file_entry.value("path", "");
            std::string manifest_hash = file_entry.value("hash", "");
            
            if (path.empty() || manifest_hash.empty()) continue;
            
            std::string content = fs.readFile(path);
            std::string current_hash_file = frans::application::fnv1a_64_hex(content);
            
            if (current_hash_file != manifest_hash) return Freshness::Stale;
        }
        
        return Freshness::Fresh;
    } catch (...) {
        return Freshness::Stale;
    }
}

// ===========================================================================
// Update UI helpers
// ===========================================================================
void PromptDialog::updateKeyStatus() {
    frans::application::ApiKeyOptions key_opt;
    key_opt.key_file = cfg_.gemini_api_key_file;
    auto key_result = frans::application::ApiKeyProvider::resolve(key_opt);
    
    if (key_result.found()) {
        std::string masked = frans::application::maskApiKey(key_result.key);
        if (key_status_box_) {
            std::string label = "key: " + masked + " | model: " + cfg_.gemini_model;
            key_status_box_->copy_label(label.c_str());
            key_status_box_->labelcolor(FL_DARK_GREEN);
        }
        if (select_btn_) select_btn_->activate();
    } else {
        if (key_status_box_) {
            key_status_box_->copy_label("key: tidak ditemukan");
            key_status_box_->labelcolor(FL_RED);
        }
        if (select_btn_) select_btn_->deactivate();
    }
}

void PromptDialog::updateModeDisplay() {
    if (!mode_box_) return;
    
    if (last_selection_.files.empty()) {
        mode_box_->copy_label("Mode: - | Token: -");
        return;
    }
    
    std::string mode_str = last_selection_.mode.empty() ? "-" : last_selection_.mode;
    std::string token_str = formatBytesHelper(last_selection_.est_tokens * 4) + " (~" +
                            std::to_string(last_selection_.est_tokens) + " token)";
    std::string label = "Mode: " + mode_str + " | Dikirim: " +
                        formatBytesHelper(last_selection_.sent_bytes) + " | " + token_str;
    mode_box_->copy_label(label.c_str());
}

void PromptDialog::updateBrowserFromSelection() {
    if (!file_browser_) return;
    
    file_browser_->clear();
    
    for (const auto& entry : last_selection_.files) {
        std::string prio = entry.priority;
        if (prio.empty()) prio = "?";
        
        std::string item = "[" + prio + "] " + entry.path +
                           " (" + formatBytesHelper(entry.size_bytes) + ")";
        if (!entry.reason.empty()) {
            item += " - " + entry.reason;
        }
        
        file_browser_->add(item.c_str());
        int idx = file_browser_->nitems();
        file_browser_->checked(idx, entry.selected);
    }
    
    updateTotals();
}

void PromptDialog::updateTotals() {
    if (!total_box_) return;
    
    std::size_t total_files = 0;
    std::size_t total_bytes = 0;
    
    for (const auto& entry : last_selection_.files) {
        if (entry.selected) {
            ++total_files;
            total_bytes += entry.size_bytes;
        }
    }
    
    std::string label = "Total terpilih: " + std::to_string(total_files) +
                        " file, " + formatBytesHelper(total_bytes);
    total_box_->copy_label(label.c_str());
}

void PromptDialog::updateFreshnessDisplay() {
    if (!freshness_box_) return;
    
    Freshness fresh = checkFreshness();
    
    switch (fresh) {
        case Freshness::Fresh:
            freshness_box_->copy_label("Status: SEGAR \xE2\x9C\x93");
            freshness_box_->labelcolor(FL_DARK_GREEN);
            has_fresh_source_code_ = true;
            if (location_box_) {
                std::string loc = "Lokasi: " + last_build_result_.source_code_path;
                location_box_->copy_label(loc.c_str());
            }
            if (open_folder_btn_) open_folder_btn_->activate();
            break;
            
        case Freshness::Stale:
            freshness_box_->copy_label("Status: BASI !");
            freshness_box_->labelcolor(FL_RED);
            has_fresh_source_code_ = false;
            break;
            
        case Freshness::NoFile:
            freshness_box_->copy_label("Status: belum ada");
            freshness_box_->labelcolor(FL_DARK3);
            has_fresh_source_code_ = false;
            if (location_box_) location_box_->copy_label("Lokasi: -");
            if (open_folder_btn_) open_folder_btn_->deactivate();
            break;
    }
}

void PromptDialog::setStatus(const std::string& msg, unsigned color) {
    if (!status_box_) return;
    status_box_->copy_label(msg.c_str());
    status_box_->labelcolor(static_cast<Fl_Color>(color));
    status_box_->redraw();
}

// ===========================================================================
// Template loading
// ===========================================================================
std::string PromptDialog::loadTemplate() {
    const std::string path = cfg_.prompt_template_path;
    if (!path.empty()) {
        std::string contents = readFileContents(path);
        if (!contents.empty()) return contents;
    }
    return frans::application::defaultPromptTemplate();
}

} // namespace frans::ui