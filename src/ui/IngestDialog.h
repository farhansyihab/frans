// src/ui/IngestDialog.h
//
// Fase B #50 -- dialog untuk ingest penuh repo ke markdown.
#pragma once
#include "application/ingest/RepoIngester.h"
#include "config/AppConfig.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <string>

class Fl_Input;
class Fl_Button;
class Fl_Check_Button;
class Fl_Progress;
class Fl_Text_Display;
class Fl_Text_Buffer;

namespace frans::ui {

class IngestDialog {
public:
    // Show modal dialog. Returns when dialog is closed.
    static void show(const std::string& initial_root,
                     const frans::config::AppConfig& cfg);
    
    IngestDialog(const IngestDialog&) = delete;
    IngestDialog& operator=(const IngestDialog&) = delete;
    
private:
    IngestDialog(std::string initial_root, const frans::config::AppConfig& cfg);
    ~IngestDialog();
    
    void build();
    void doBrowse();
    void doGenerate();
    void doCancel();
    void doOpenFolder();
    void doClose();
    
    void updateProgress();
    void appendLog(const std::string& line);
    void setStatus(const std::string& msg, unsigned color);
    void setButtonsEnabled(bool generate_enabled, bool cancel_enabled);
    
    static void cbBrowse(Fl_Widget*, void*);
    static void cbGenerate(Fl_Widget*, void*);
    static void cbCancel(Fl_Widget*, void*);
    static void cbOpenFolder(Fl_Widget*, void*);
    static void cbClose(Fl_Widget*, void*);
    static void onPoll(void* data);
    
    Fl_Window* win_ = nullptr;
    Fl_Input* root_input_ = nullptr;
    Fl_Button* browse_btn_ = nullptr;
    Fl_Check_Button* exclude_secrets_chk_ = nullptr;
    Fl_Input* max_total_mb_input_ = nullptr;
    Fl_Button* generate_btn_ = nullptr;
    Fl_Button* cancel_btn_ = nullptr;
    Fl_Progress* progress_bar_ = nullptr;
    Fl_Text_Display* log_display_ = nullptr;
    Fl_Text_Buffer* log_buffer_ = nullptr;
    Fl_Button* open_folder_btn_ = nullptr;
    Fl_Button* close_btn_ = nullptr;
    
    std::string current_root_;
    const frans::config::AppConfig& cfg_;
    
    frans::application::RepoIngester ingester_;
    bool is_running_ = false;
    
    static constexpr double kPollInterval = 0.1;  // 100ms
};

} // namespace frans::ui