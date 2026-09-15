// ~/AgenAI/frans/src/ui/AgentPanel.h
#pragma once

#include "application/task_runner/TaskRunner.h"

#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Multiline_Input.H>

#include <functional>
#include <memory>
#include <string>

class Fl_Text_Display;
class Fl_Text_Buffer;
class Fl_Widget;

namespace frans::application {
struct AppContext;
class ProviderRouter;
}

namespace frans::infrastructure {
class SandboxedFileSystem;
class LlamaCppProvider;
class DeepSeekProvider;
}

namespace frans::ui {

// Panel untuk menjalankan Agent task secara end-to-end.
//
// Stage 7f: tambah ProjectGraphCache (via AppContext) + TargetScopeResolver
// untuk auto-detect entry point dan in-scope files.
// Stage 7g: tambah toggle [Log]/[Diff] + render unified diff setelah run.
// Stage 9c: tambah ProviderRouter + DailyBudgetGuard untuk hybrid routing,
// plus label read-only untuk daily API budget.
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
    static void onLogViewClicked(Fl_Widget*, void*);
    static void onDiffViewClicked(Fl_Widget*, void*);

    void startTask();
    void renderResult(const frans::application::TaskProgress& p);
    void renderDiff(const frans::application::TaskProgress& p);   // Stage 7g

    // Stage 7f — update label "Detected scope: N file(s)" berdasarkan
    // task & root input saat ini. Dipanggil dari tick() saat idle.
    void updateScopeLabel(const std::string& task, const std::string& root);

    // Stage 9c — update label "Daily API budget: ..." dari
    // ctx_->budget_guard->status(). Dipanggil dari tick() saat idle.
    void updateBudgetLabel();

    // ---- deps ----
    frans::application::AppContext* ctx_;
    Deps                            deps_;

    // ---- widgets ----
    Fl_Input*           root_input_    = nullptr;
    Fl_Button*          browse_btn_    = nullptr;
    Fl_Multiline_Input* task_input_    = nullptr;
    Fl_Input*           target_input_  = nullptr;
    Fl_Box*             scope_label_   = nullptr;   // Stage 7f
    Fl_Box*             budget_label_  = nullptr;   // Stage 9c
    Fl_Button*          run_btn_       = nullptr;
    Fl_Button*          cancel_btn_    = nullptr;
    Fl_Button*          clear_btn_     = nullptr;
    Fl_Box*             status_label_  = nullptr;

    // Stage 7g — toggle [Log]/[Diff] + dua view yang overlap.
    Fl_Light_Button*    btn_log_view_  = nullptr;
    Fl_Light_Button*    btn_diff_view_ = nullptr;
    Fl_Text_Display*    output_        = nullptr;
    Fl_Text_Buffer*     output_buf_    = nullptr;
    Fl_Text_Display*    diff_view_     = nullptr;
    Fl_Text_Buffer*     diff_buf_      = nullptr;

    // ---- state ----
    std::unique_ptr<frans::application::TaskRunner>                runner_;
    std::unique_ptr<frans::infrastructure::SandboxedFileSystem>    fs_;
    std::unique_ptr<frans::infrastructure::LlamaCppProvider>       provider_;

    // Stage 9c — paid provider + router. Dibangun ulang setiap startTask().
    std::unique_ptr<frans::infrastructure::DeepSeekProvider>       deepseek_provider_;
    std::unique_ptr<frans::application::ProviderRouter>            router_;

    bool        result_shown_ = false;
    std::string status_buf_;

    // Stage 7f — budget token untuk context selection. Dihitung sekali
    // di constructor lewat ContextBudgetManager, TIDAK di-hardcode.
    int         code_budget_ = 0;

    // Stage 7f — tracking untuk hindari resolve berulang di tick().
    std::string last_scope_task_;
    std::string last_scope_root_;

    // Stage 9c — tracking supaya label budget hanya di-redraw saat nilai
    // berubah (menghindari spam redraw tiap tick 100ms).
    std::string last_budget_str_;
};

} // namespace frans::ui
