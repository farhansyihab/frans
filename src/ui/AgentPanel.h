// ~/AgenAI/frans/src/ui/AgentPanel.h
#pragma once

#include "application/task_runner/TaskRunner.h"

#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>

#include <functional>
#include <memory>
#include <string>

class Fl_Text_Display;
class Fl_Text_Buffer;
class Fl_Widget;

namespace frans::application {
struct AppContext;
}

namespace frans::infrastructure {
class SandboxedFileSystem;
class LlamaCppProvider;
}

namespace frans::ui {

// Panel untuk menjalankan Agent task secara end-to-end.
//
// Tanggung jawab:
//   - Project root picker
//   - Task description input (multi-line)
//   - Target entry point input
//   - Run / Cancel / Clear buttons
//   - Status + result display
//   - Confirm marshalling ke main thread (lewat TaskRunner)
//
// Pola threading:
//   - tick() dipanggil dari timer main-thread.
//   - TaskRunner menjalankan Agent di worker thread.
//   - Semua widget HANYA disentuh di tick() / callback (main thread).
class AgentPanel {
public:
    struct Deps {
        std::function<bool()>        is_server_running;
        std::function<std::string()> get_host;
        std::function<int()>         get_port;
        std::function<void()>        on_activity;
    };

    AgentPanel(frans::application::AppContext* ctx, Deps deps);
    ~AgentPanel();

    AgentPanel(const AgentPanel&)            = delete;
    AgentPanel& operator=(const AgentPanel&) = delete;

    // Bangun widget. Return y akhir (bottom + margin).
    int createWidgets(int x, int y, int w);

    // Dipanggil dari timer main-thread (mis. 100ms).
    void tick();

    void setEnabled(bool on);

private:
    static void onBrowseClicked(Fl_Widget*, void*);
    static void onRunClicked(Fl_Widget*, void*);
    static void onCancelClicked(Fl_Widget*, void*);
    static void onClearClicked(Fl_Widget*, void*);

    void startTask();
    void renderResult(const frans::application::TaskProgress& p);

    // ---- deps ----
    frans::application::AppContext* ctx_;
    Deps                            deps_;

    // ---- widgets ----
    Fl_Input*           root_input_    = nullptr;
    Fl_Button*          browse_btn_    = nullptr;
    Fl_Multiline_Input* task_input_    = nullptr;
    Fl_Input*           target_input_  = nullptr;
    Fl_Button*          run_btn_       = nullptr;
    Fl_Button*          cancel_btn_    = nullptr;
    Fl_Button*          clear_btn_     = nullptr;
    Fl_Box*             status_label_  = nullptr;
    Fl_Text_Display*    output_        = nullptr;
    Fl_Text_Buffer*     output_buf_    = nullptr;

    // ---- state ----
    std::unique_ptr<frans::application::TaskRunner>                runner_;
    std::unique_ptr<frans::infrastructure::SandboxedFileSystem>    fs_;
    std::unique_ptr<frans::infrastructure::LlamaCppProvider>       provider_;

    bool        result_shown_ = false;
    std::string status_buf_;
};

} // namespace frans::ui
