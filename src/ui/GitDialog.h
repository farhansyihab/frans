// src/ui/GitDialog.h
#pragma once

#include "config/AppConfig.h"
#include "ui/SessionState.h"
#include "application/git/GitFacade.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Window.H>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace frans::ui {

class GitDialog {
public:
    static void show(const std::string& project_root,
                     SessionState* session,
                     const frans::config::AppConfig& cfg);

private:
    GitDialog(const std::string& project_root,
              SessionState* session,
              const frans::config::AppConfig& cfg);
    ~GitDialog();

    GitDialog(const GitDialog&)            = delete;
    GitDialog& operator=(const GitDialog&) = delete;

    void createUI();
    void layout();

    void refreshStatus();
    void doAddAll();
    void doCommit();
    void doPush();

    void setButtonsEnabled(bool enabled);
    void appendOutput(const std::string& text);
    void clearOutput();
    void updateStatusDisplay();
    void updateChangesBrowser();

    bool hasSuspiciousFiles() const;
    std::string buildSuspiciousWarning() const;

    // Worker infrastructure
    void startWorker(std::function<void()> task,
                     std::function<void()> on_done);
    void cancelWorker();
    void onWorkerFinished();
    static void pollWorker(void* data);

    // FLTK callbacks
    static void cbRefresh(Fl_Widget*, void*);
    static void cbAddAll(Fl_Widget*, void*);
    static void cbCommit(Fl_Widget*, void*);
    static void cbPush(Fl_Widget*, void*);
    static void cbCancel(Fl_Widget*, void*);
    static void cbClose(Fl_Widget*, void*);

    Fl_Window*    win_                 = nullptr;
    Fl_Box*       status_label_        = nullptr;
    Fl_Browser*   changes_browser_     = nullptr;
    Fl_Input*     commit_message_input_= nullptr;
    Fl_Output*    output_              = nullptr;

    Fl_Button*    btn_refresh_         = nullptr;
    Fl_Button*    btn_add_all_         = nullptr;
    Fl_Button*    btn_commit_          = nullptr;
    Fl_Button*    btn_push_            = nullptr;
    Fl_Button*    btn_cancel_          = nullptr;
    Fl_Button*    btn_close_           = nullptr;

    std::string   project_root_;
    SessionState* session_             = nullptr;
    frans::config::AppConfig cfg_;

    std::unique_ptr<frans::application::GitFacade> git_;
    
    // State untuk komunikasi thread-safe antara worker dan main thread
    frans::application::GitStatus   current_status_;
    frans::application::GitOpResult last_op_result_;

    std::thread              worker_thread_;
    std::atomic<bool>        cancel_flag_{false};
    std::atomic<bool>        worker_running_{false};
    std::function<void()>    worker_callback_;
};

} // namespace frans::ui