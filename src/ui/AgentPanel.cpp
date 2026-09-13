// ~/AgenAI/frans/src/ui/AgentPanel.cpp
#include "ui/AgentPanel.h"

#include "application/AppContext.h"
#include "application/agent/AgentContext.h"
#include "application/agent/Agent.h"
#include "domain/fs/IFileSystemPort.h"
#include "domain/llm/ILlmProvider.h"
#include "infrastructure/fs/SandboxedFileSystem.h"
#include "infrastructure/llama/LlamaCppProvider.h"
#include "infrastructure/llama/LlamaServerProcessController.h"

#include <FL/Fl.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/fl_ask.H>

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <utility>

namespace frans::ui {

using frans::application::Agent;
using frans::application::AgentContext;
using frans::application::AppContext;
using frans::application::TaskProgress;
using frans::application::TaskRunner;
using frans::infrastructure::LlamaCppProvider;
using frans::infrastructure::SandboxedFileSystem;

namespace {

constexpr int kBtnH = 28;

std::string humanBytes(std::size_t n) {
    if (n < 1024) return std::to_string(n) + " B";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(n) / 1024.0);
    return buf;
}

// Ambil beberapa baris terakhir stderr untuk ringkasan.
std::string tailLines(const std::string& s, std::size_t max_lines) {
    if (s.empty()) return {};
    std::vector<std::size_t> newlines;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') newlines.push_back(i);
    }
    if (newlines.size() <= max_lines) return s;
    const std::size_t start = newlines[newlines.size() - max_lines - 1] + 1;
    return s.substr(start);
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
AgentPanel::AgentPanel(AppContext* ctx, Deps deps)
    : ctx_(ctx), deps_(std::move(deps))
{
    runner_ = std::make_unique<TaskRunner>();
}

AgentPanel::~AgentPanel() {
    if (runner_) {
        runner_->requestCancel();
    }
    // TaskRunner destructor akan join thread.
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
int AgentPanel::createWidgets(int x, int y, int w) {
    int by = y;

    // ---- Title ----
    Fl_Box* title = new Fl_Box(x, by, w, 20, "Agent Task Runner");
    title->labelfont(FL_BOLD);
    title->labelsize(12);
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    by += 22;

    // ---- Project root row ----
    Fl_Box* lbl_root = new Fl_Box(x, by, 100, 20, "Project root:");
    lbl_root->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lbl_root->labelsize(11);

    root_input_ = new Fl_Input(x + 100, by, w - 100 - 80, kBtnH, nullptr);
    root_input_->value("~/AgenAI/frans/proyek/test");
    root_input_->tooltip("Folder kerja Agent. Folder akan dibuat kalau belum ada.");

    browse_btn_ = new Fl_Button(x + w - 76, by, 76, kBtnH, "Browse");
    browse_btn_->callback(onBrowseClicked, this);

    by += kBtnH + 4;

    // ---- Task description ----
    Fl_Box* lbl_task = new Fl_Box(x, by, 100, 20, "Task:");
    lbl_task->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lbl_task->labelsize(11);

    task_input_ = new Fl_Multiline_Input(x + 100, by, w - 100, 60, nullptr);
    task_input_->value(
        "Create a simple Python Hello World program with a function hello() "
        "that returns the string \"Hello, World!\", and a main block that "
        "prints it.");
    task_input_->tooltip("Deskripsi task dalam bahasa natural (1-3 baris).");

    by += 60 + 4;

    // ---- Target file ----
    Fl_Box* lbl_target = new Fl_Box(x, by, 100, 20, "Target file:");
    lbl_target->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    lbl_target->labelsize(11);

    target_input_ = new Fl_Input(x + 100, by, 240, kBtnH, nullptr);
    target_input_->value("hello.py");
    target_input_->tooltip("Entry point relatif terhadap project root.");

    by += kBtnH + 6;

    // ---- Buttons ----
    run_btn_ = new Fl_Button(x, by, 130, kBtnH, "Run Task");
    run_btn_->callback(onRunClicked, this);

    cancel_btn_ = new Fl_Button(x + 140, by, 100, kBtnH, "Cancel");
    cancel_btn_->callback(onCancelClicked, this);
    cancel_btn_->deactivate();

    clear_btn_ = new Fl_Button(x + 250, by, 100, kBtnH, "Clear Output");
    clear_btn_->callback(onClearClicked, this);

    by += kBtnH + 6;

    // ---- Status ----
    status_label_ = new Fl_Box(x, by, w, 22, "Idle.");
    status_label_->box(FL_DOWN_BOX);
    status_label_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    status_label_->labelsize(11);
    by += 26;

    // ---- Output ----
    const int out_h = 220;
    output_ = new Fl_Text_Display(x, by, w, out_h, nullptr);
    output_buf_ = new Fl_Text_Buffer();
    output_->buffer(output_buf_);
    output_->textfont(FL_COURIER);
    output_->textsize(10);
    output_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    output_->color(FL_WHITE);

    by += out_h + 4;

    return by;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void AgentPanel::setEnabled(bool on) {
    auto apply = [on](Fl_Widget* w) {
        if (!w) return;
        if (on) w->activate();
        else    w->deactivate();
    };
    apply(root_input_);
    apply(browse_btn_);
    apply(task_input_);
    apply(target_input_);
    apply(run_btn_);
    apply(clear_btn_);

    // Cancel button state ditangani oleh tick() berdasarkan status runner.
}

void AgentPanel::tick() {
    if (!runner_) return;

    // 1. Poll progress.
    runner_->poll();
    const TaskProgress p = runner_->snapshot();

    // 2. Update status label.
    {
        std::ostringstream oss;
        if (p.running) {
            oss << "RUNNING  |  state="
                << (p.current_state.empty() ? "(starting)" : p.current_state)
                << "  attempt=" << (p.attempt + 1) << "/" << p.max_attempts
                << "  elapsed=" << p.elapsed_sec << "s";
        } else if (p.done) {
            if (p.cancelled) {
                oss << "CANCELLED  |  attempts=" << p.history.size()
                    << "  elapsed=" << p.elapsed_sec << "s";
            } else if (p.success) {
                oss << "SUCCESS  |  attempts=" << p.history.size()
                    << "  elapsed=" << p.elapsed_sec << "s";
            } else {
                oss << "FAILED  |  attempts=" << p.history.size()
                    << "  elapsed=" << p.elapsed_sec << "s";
            }
        } else {
            oss << "Idle.";
        }
        const std::string s = oss.str();
        if (s != status_buf_ && status_label_) {
            status_buf_ = s;
            status_label_->copy_label(status_buf_.c_str());
            status_label_->redraw();
        }
    }

    // 3. Enable/disable buttons.
    if (run_btn_) {
        if (p.running) run_btn_->deactivate();
        else           run_btn_->activate();
    }
    if (cancel_btn_) {
        if (p.running) cancel_btn_->activate();
        else           cancel_btn_->deactivate();
    }

    // 4. Pending confirmation?
    {
        const std::string msg = runner_->takePendingConfirmMessage();
        if (!msg.empty()) {
            const int ans = fl_choice(
                "%s\n\nLanjutkan eksekusi?",
                "Batal", "Ya, lanjut",
                nullptr, msg.c_str());
            runner_->resolvePendingConfirm(ans == 1);
        }
    }

    // 5. Render result (edge-triggered).
    if (p.done && !result_shown_) {
        result_shown_ = true;
        renderResult(p);
    }
}

// ---------------------------------------------------------------------------
// Private: startTask
// ---------------------------------------------------------------------------
void AgentPanel::startTask() {
    if (!ctx_ || !ctx_->llama) {
        fl_message("AppContext/llama belum siap.");
        return;
    }
    if (runner_->isRunning()) {
        fl_message("Task masih berjalan.");
        return;
    }

    const std::string root   = root_input_ ? root_input_->value()    : "";
    const std::string task   = task_input_ ? task_input_->value()    : "";
    const std::string target = target_input_ ? target_input_->value(): "";

    if (root.empty())   { fl_message("Project root kosong."); return; }
    if (task.empty())   { fl_message("Task description kosong."); return; }
    if (target.empty()) { fl_message("Target file kosong."); return; }

    if (deps_.is_server_running && !deps_.is_server_running()) {
        fl_message("Llama server tidak berjalan.\n"
                   "Klik \"Start Server\" di panel Llama dulu.");
        return;
    }

    // Resolve "~" ke $HOME.
    std::string resolved_root = root;
    if (!resolved_root.empty() && resolved_root[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) resolved_root = std::string(home) + resolved_root.substr(1);
    }

    // Buat sandbox (membuat folder kalau belum ada).
    try {
        fs_ = std::make_unique<SandboxedFileSystem>(resolved_root);
    } catch (const std::exception& e) {
        fl_message("Gagal membuat sandbox:\n%s", e.what());
        return;
    }
    std::fprintf(stderr,
        "[frans][agent_panel] sandbox root = %s\n",
        fs_->root().string().c_str());
    std::fflush(stderr);

    // Buat provider.
    LlamaCppProvider::Config cfg;
    cfg.host = deps_.get_host ? deps_.get_host() : std::string("127.0.0.1");
    cfg.port = deps_.get_port ? deps_.get_port() : 8080;
    provider_ = std::make_unique<LlamaCppProvider>(cfg);

    // Reset UI state.
    result_shown_ = false;
    if (output_buf_) output_buf_->text("");

    if (deps_.on_activity) deps_.on_activity();

    // Build Agent::Deps.
    Agent::Deps adeps;
    adeps.provider            = provider_.get();
    adeps.fs                  = fs_.get();
    adeps.graph               = nullptr;   // MVP 7e: tanpa context selection
    adeps.sandbox_root        = fs_->root().string();
    adeps.execute_timeout_sec = 30;
    // confirm & cancel_flag di-override oleh TaskRunner.

    AgentContext actx;
    actx.taskDescription    = task;
    actx.targetRelativePath = target;

    std::fprintf(stderr,
        "[frans][agent_panel] start task: root=%s target=%s\n",
        fs_->root().string().c_str(), target.c_str());
    std::fflush(stderr);

    if (!runner_->start(std::move(adeps), std::move(actx))) {
        fl_message("TaskRunner sudah menjalankan task lain.");
        return;
    }

    if (output_buf_) {
        std::string hdr = "=== Starting agent task ===\n";
        hdr += "Project root: " + fs_->root().string() + "\n";
        hdr += "Target file : " + target + "\n";
        hdr += "Task        : " + task + "\n\n";
        output_buf_->append(hdr.c_str());
    }
}

// ---------------------------------------------------------------------------
// Private: renderResult
// ---------------------------------------------------------------------------
void AgentPanel::renderResult(const TaskProgress& p) {
    if (!output_buf_) return;

    std::ostringstream oss;

    oss << "=== Result ===\n";
    if (p.cancelled) {
        oss << "Status: CANCELLED\n";
    } else if (p.success) {
        oss << "Status: SUCCESS\n";
    } else {
        oss << "Status: FAILED\n";
    }
    oss << "Attempts: " << p.history.size()
        << " / " << p.max_attempts << "\n";
    oss << "Elapsed : " << p.elapsed_sec << "s\n";

    if (!p.final_error.empty() && !p.success && !p.cancelled) {
        oss << "\nFinal error: " << p.final_error << "\n";
    }

    // Attempt history.
    oss << "\n--- Attempt history ---\n";
    for (const auto& a : p.history) {
        oss << "Attempt " << a.attempt << ": "
            << "exit=" << a.exit_code
            << (a.timed_out ? " [TIMEOUT]" : "")
            << "\n";
        if (!a.observation.empty()) {
            oss << "  " << a.observation.substr(0, 400) << "\n";
        }
    }

    // Files written.
    if (!p.files.empty()) {
        oss << "\n--- Files written ---\n";
        for (const auto& f : p.files) {
            oss << "  " << f.relative_path
                << "  (" << humanBytes(f.content.size()) << ")\n";
        }
    }

    // Final stdout/stderr.
    if (!p.stdout_text.empty()) {
        oss << "\n--- Final stdout ---\n";
        oss << p.stdout_text;
        if (p.stdout_text.back() != '\n') oss << "\n";
    }
    if (!p.stderr_text.empty() && !p.success) {
        oss << "\n--- Final stderr (tail) ---\n";
        oss << tailLines(p.stderr_text, 15);
        if (!p.stderr_text.empty() && p.stderr_text.back() != '\n') {
            oss << "\n";
        }
    }

    output_buf_->append(oss.str().c_str());
    if (output_) {
        output_->insert_position(output_buf_->length());
        output_->show_insert_position();
    }
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void AgentPanel::onBrowseClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    const char* start = self->root_input_ ? self->root_input_->value() : nullptr;
    const char* dir = fl_dir_chooser("Pilih project root", start, /*show_files=*/0);
    if (dir && *dir && self->root_input_) {
        self->root_input_->value(dir);
    }
}

void AgentPanel::onRunClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    self->startTask();
}

void AgentPanel::onCancelClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    if (self->runner_) {
        self->runner_->requestCancel();
    }
}

void AgentPanel::onClearClicked(Fl_Widget*, void* data) {
    auto* self = static_cast<AgentPanel*>(data);
    if (self->runner_ && self->runner_->isRunning()) return;
    if (self->output_buf_) self->output_buf_->text("");
    self->result_shown_ = false;
}

} // namespace frans::ui
