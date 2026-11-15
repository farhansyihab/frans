// src/ui/PromptDialog_Callbacks.cpp
// Static callback wrappers and polling callbacks
#include "ui/PromptDialog.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Browser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace frans::ui {

namespace {

std::string normalizeModelName(const std::string& raw) {
    std::string s = raw;
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    
    const std::string prefix = "models/";
    if (s.size() > prefix.size() && s.compare(0, prefix.size(), prefix) == 0) {
        s = s.substr(prefix.size());
    }
    return s;
}

bool isModelNotFoundError(const std::string& error) {
    std::string lower = error;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    return lower.find("model") != std::string::npos &&
           (lower.find("not found") != std::string::npos ||
            lower.find("tidak ditemukan") != std::string::npos ||
            lower.find("404") != std::string::npos ||
            lower.find("does not exist") != std::string::npos ||
            lower.find("invalid model") != std::string::npos ||
            lower.find("modelnotfound") != std::string::npos);
}

std::string findSimilarModel(const std::string& target,
                             const std::vector<frans::application::ModelInfo>& models,
                             int max_suggestions = 5) {
    std::string target_lower = target;
    std::transform(target_lower.begin(), target_lower.end(), target_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    struct Scored {
        std::string name;
        int score = 0;
    };
    
    std::vector<Scored> scored;
    scored.reserve(models.size());
    
    for (const auto& m : models) {
        std::string name = normalizeModelName(m.name);
        std::string name_lower = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        
        int score = 0;
        std::size_t common = 0;
        while (common < target_lower.size() && common < name_lower.size() &&
               target_lower[common] == name_lower[common]) {
            ++common;
        }
        score += static_cast<int>(common) * 2;
        
        if (name_lower.find(target_lower) != std::string::npos ||
            target_lower.find(name_lower) != std::string::npos) {
            score += 20;
        }
        
        int len_diff = std::abs(static_cast<int>(name_lower.size()) -
                                static_cast<int>(target_lower.size()));
        if (len_diff < 5) score += 5;
        
        if (score > 0) {
            scored.push_back({name, score});
        }
    }
    
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.score > b.score; });
    
    std::ostringstream oss;
    int count = 0;
    for (const auto& s : scored) {
        if (count >= max_suggestions) break;
        oss << "  - " << s.name << "\n";
        ++count;
    }
    
    return oss.str();
}

void showScrollableMessage(const char* title, const std::string& message) {
    constexpr int dlg_w = 650;
    constexpr int dlg_h = 500;
    constexpr int pad   = 10;
    constexpr int btn_w = 80;
    constexpr int btn_h = 30;
    
    Fl_Window* win = new Fl_Window(dlg_w, dlg_h, title);
    win->set_modal();
    win->begin();
    
    Fl_Text_Display* display = new Fl_Text_Display(
        pad, pad, dlg_w - 2 * pad, dlg_h - 2 * pad - btn_h - pad);
    Fl_Text_Buffer* buf = new Fl_Text_Buffer();
    display->buffer(buf);
    display->textfont(FL_COURIER);
    display->textsize(11);
    display->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    display->color(FL_WHITE);
    buf->text(message.c_str());
    
    Fl_Button* ok_btn = new Fl_Button(
        dlg_w - pad - btn_w, dlg_h - pad - btn_h, btn_w, btn_h, "OK");
    ok_btn->callback([](Fl_Widget* w, void*) {
        if (Fl_Window* win = w ? w->window() : nullptr) {
            win->hide();
        }
    });
    ok_btn->take_focus();
    
    win->end();
    win->resizable(display);
    win->show();
    
    while (win->shown()) Fl::wait();
    
    delete buf;
    delete win;
}

} // namespace

// ===========================================================================
// Callbacks
// ===========================================================================
void PromptDialog::cbGenerate(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doGenerate();
}

void PromptDialog::cbCopy(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doCopy();
}

void PromptDialog::cbFetchLastError(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doFetchLastError();
}

void PromptDialog::cbTestConnection(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doTestConnection();
}

void PromptDialog::cbSelectFiles(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doSelectFiles();
}

void PromptDialog::cbAddManual(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doAddManual();
}

void PromptDialog::cbCancelSelect(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doCancelSelect();
}

void PromptDialog::cbBrowserChanged(Fl_Widget*, void* data) {
    auto* dlg = static_cast<PromptDialog*>(data);
    if (!dlg || !dlg->file_browser_) return;
    
    int n = dlg->file_browser_->nitems();
    for (int i = 0; i < n && i < static_cast<int>(dlg->last_selection_.files.size()); ++i) {
        dlg->last_selection_.files[i].selected = dlg->file_browser_->checked(i + 1) != 0;
    }
    dlg->updateTotals();
}

void PromptDialog::cbBuildSourceCode(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doBuildSourceCode();
}

void PromptDialog::cbOpenFolder(Fl_Widget*, void* data) {
    static_cast<PromptDialog*>(data)->doOpenFolder();
}

void PromptDialog::cbClose(Fl_Widget* w, void*) {
    if (Fl_Window* win = w ? w->window() : nullptr) {
        win->hide();
    }
}

// ===========================================================================
// Polling callbacks
// ===========================================================================
void PromptDialog::cbPollListModelsWorker(void* data) {
    auto* dlg = static_cast<PromptDialog*>(data);
    if (!dlg) return;
    
    if (dlg->list_models_worker_running_.load()) {
        Fl::repeat_timeout(0.1, cbPollListModelsWorker, data);
        return;
    }
    
    if (dlg->list_models_worker_thread_.joinable()) {
        dlg->list_models_worker_thread_.join();
    }
    
    if (dlg->test_conn_btn_) dlg->test_conn_btn_->activate();
    
    if (dlg->list_models_worker_ok_) {
        dlg->setStatus("Tes koneksi berhasil.", FL_DARK_GREEN);
        showScrollableMessage("Daftar Model Gemini", dlg->list_models_worker_message_);
    } else {
        dlg->setStatus("Tes koneksi gagal.", FL_RED);
        showScrollableMessage("Tes Koneksi Gagal", dlg->list_models_worker_message_);
    }
    
    dlg->list_models_worker_message_.clear();
    dlg->list_models_worker_ok_ = false;
}

void PromptDialog::cbPollSelectWorker(void* data) {
    auto* dlg = static_cast<PromptDialog*>(data);
    if (!dlg) return;
    
    if (dlg->select_worker_running_.load()) {
        Fl::repeat_timeout(0.1, cbPollSelectWorker, data);
        return;
    }
    
    if (dlg->select_worker_thread_.joinable()) {
        dlg->select_worker_thread_.join();
    }
    
    if (dlg->select_btn_) dlg->select_btn_->activate();
    if (dlg->cancel_select_btn_) dlg->cancel_select_btn_->deactivate();
    
    if (dlg->select_worker_ok_) {
        dlg->last_selection_ = dlg->select_worker_result_;
        dlg->last_collected_files_ = dlg->select_worker_collected_;
        dlg->updateBrowserFromSelection();
        dlg->updateModeDisplay();
        dlg->setStatus("Seleksi file selesai. Tinjau dan koreksi centang.", FL_DARK_GREEN);
    } else {
        dlg->setStatus("Seleksi file gagal: " + dlg->select_worker_error_, FL_RED);
        
        std::string error_msg = dlg->select_worker_error_;
        if (isModelNotFoundError(error_msg)) {
            std::ostringstream oss;
            oss << "GEMINI MODEL TIDAK DITEMUKAN\n";
            oss << "================================\n\n";
            oss << "Model yang dipakai: " << dlg->select_worker_used_model_ << "\n";
            oss << "Dari config:        " << dlg->cfg_.gemini_model << "\n\n";
            oss << "API key Anda VALID (Tes Koneksi berhasil dengan "
                << dlg->list_models_worker_models_.size() << " model),\n";
            oss << "tetapi nama model di config/app.json tidak ada di daftar.\n\n";
            oss << "SOLUSI:\n";
            oss << "1. Klik [Tes Koneksi / Daftar Model] untuk melihat nama model yang valid.\n";
            oss << "2. Edit config/app.json, ganti 'gemini_model' dengan nama yang valid.\n";
            oss << "3. Contoh: \"gemini_model\": \"gemini-3.8-flash\"\n\n";
            
            if (!dlg->list_models_worker_models_.empty()) {
                std::string similar = findSimilarModel(
                    dlg->select_worker_used_model_,
                    dlg->list_models_worker_models_);
                if (!similar.empty()) {
                    oss << "Model yang MIRIP dengan '" << dlg->select_worker_used_model_ << "':\n";
                    oss << similar;
                    oss << "\n";
                }
            } else {
                oss << "Tip: Klik [Tes Koneksi] dulu untuk mengisi daftar model.\n";
            }
            
            oss << "\nError detail dari Gemini:\n" << error_msg << "\n";
            showScrollableMessage("Model Gemini Tidak Ditemukan", oss.str());
        } else {
            fl_message("Seleksi file gagal:\n%s", error_msg.c_str());
        }
    }
    
    dlg->select_worker_error_.clear();
    dlg->select_worker_ok_ = false;
}

void PromptDialog::cbPollBuildWorker(void* data) {
    auto* dlg = static_cast<PromptDialog*>(data);
    if (!dlg) return;
    
    if (dlg->build_worker_running_.load()) {
        Fl::repeat_timeout(0.1, cbPollBuildWorker, data);
        return;
    }
    
    if (dlg->build_worker_thread_.joinable()) {
        dlg->build_worker_thread_.join();
    }
    
    if (dlg->build_btn_) dlg->build_btn_->activate();
    
    if (dlg->build_worker_result_.ok) {
        dlg->last_build_result_ = dlg->build_worker_result_;
        
        if (dlg->session_) {
            dlg->session_->last_context_path = dlg->last_build_result_.source_code_path;
        }
        
        dlg->updateFreshnessDisplay();
        
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "SourceCode.md dibangun: %zu file, %s",
                      dlg->last_build_result_.files_included,
                      dlg->formatBytes(dlg->last_build_result_.total_bytes).c_str());
        dlg->setStatus(buf, FL_DARK_GREEN);
    } else {
        dlg->setStatus("Build gagal: " + dlg->build_worker_result_.error, FL_RED);
        fl_message("Build SourceCode.md gagal:\n%s", dlg->build_worker_result_.error.c_str());
    }
}

} // namespace frans::ui